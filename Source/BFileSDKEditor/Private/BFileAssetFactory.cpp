/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#include "BFileAssetFactory.h"
#include "BFileAsset.h"
#include "BFileAssetCreator.h"
#include "BFileFinalTypes.h"
#include "BFileReader.h"
#include "BFileMeshSerialization.h"
#include "BZipFile.h"
#include "BLambdaRunnable.h"

FBFileFactoryGameThreadHandlers::FBFileFactoryGameThreadHandlers(
	TFunction<void(TFunction<void()>)> InTaskQueuer,
	TFunction<void()> InTasksDequeuerExecuter)
{
	TaskQueuer = InTaskQueuer;
	TasksDequeuerExecuter = InTasksDequeuerExecuter;
}

FBFileFactoryGameThreadHandlers::FBFileFactoryGameThreadHandlers()
{
	TaskQueuer = [this](TFunction<void()> ExecuteTask)
	{
		OptionalsNotProvided_AwaitingGameThreadTasks_Num.Increment();
		FBLambdaRunnable::RunLambdaOnGameThread([this, ExecuteTask]()
			{
				ExecuteTask();
				OptionalsNotProvided_AwaitingGameThreadTasks_Num.Decrement();
			});
	};
	TasksDequeuerExecuter = [this]()
	{
		while (OptionalsNotProvided_AwaitingGameThreadTasks_Num.GetValue() > 0)
		{
			FPlatformProcess::Sleep(0.1f);
		}
	};
}

bool FBFileAssetFactory::FactoryCreateBFileContent(
	FBFileFactoryOutputOption& Result,
	const FString& FullFilePath_ToZip, 
	FBFileFactoryGameThreadHandlers& GameThreadTaskHandlers)
{
	FString MainFileExtension = FPaths::GetExtension(FullFilePath_ToZip, false).ToLower();

	EBFileCompressionState CompressionState;

	if (MainFileExtension == "x3_p")
	{
		CompressionState = EBFileCompressionState::Uncompressed;
	}
	else if (MainFileExtension == "x3_c")
	{
		CompressionState = EBFileCompressionState::Compressed;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Unexpected extension %s"), *MainFileExtension);
		return false;
	}

	TSharedPtr<BZipArchive> Archive;
	FString ErrorMessage;
	if (!BZipFile::Open(Archive, FullFilePath_ToZip, ErrorMessage))
	{
		UE_LOG(LogTemp, Error, TEXT("An error occured during open: %s"), *ErrorMessage);
		return false;
	}

	bool bHFound = false;
	bool bGFound = false;
	bool bMFound = false;
	std::istream* HStream = nullptr;
	std::istream* GStream = nullptr;
	std::istream* MStream = nullptr;

	for (int32 i = 0; i < Archive->GetEntriesCount(); i++)
	{
		auto Entry = Archive->GetEntry(i);
		FString CurrentFileExtension = FPaths::GetExtension(Entry->GetName(), false).ToLower();

		if (CurrentFileExtension == (MainFileExtension + "_h"))
		{
			bHFound = true;
			HStream = Entry->GetDecompressionStream();
		}
		else if (CurrentFileExtension == (MainFileExtension + "_g"))
		{
			bGFound = true;
			GStream = Entry->GetDecompressionStream();
		}
		else if (CurrentFileExtension == (MainFileExtension + "_m"))
		{
			bMFound = true;
			MStream = Entry->GetDecompressionStream();
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Unexpected extension in archive %s"), *CurrentFileExtension);
			return false;
		}
	}
	if (!bHFound)
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid file: Hierarchy entry does not exist."));
		return false;
	}
	if (!bGFound)
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid file: Geometry entry does not exist."));
		return false;
	}
	if (!bMFound)
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid file: Metadata entry does not exist."));
		return false;
	}

	FBFileFactoryInputOption WithOptions(
		CompressionState,
		HStream, GStream, MStream);

	return FactoryCreateBFileContent(
		Result,
		WithOptions,
		GameThreadTaskHandlers);
}

bool FBFileAssetFactory::FactoryCreateBFileContent(
	FBFileFactoryOutputOption& Result,
	FBFileFactoryInputOption& WithOption)
{
	//This must be running in a separate thread; otherwise game-thread will wait for Lambda->RunInGameThread's result.
	check(!IsInGameThread())

	FBFileFactoryGameThreadHandlers AutoHandler;
	return FactoryCreateBFileContent(Result, WithOption, AutoHandler);
}

bool FBFileAssetFactory::FactoryCreateBFileContent(
	FBFileFactoryOutputOption& Result,
	FBFileFactoryInputOption& WithOption,
	FBFileFactoryGameThreadHandlers& GameThreadTaskHandlers)
{
	if (Result.OutputFiles.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid output: OutputFiles must be provided."));
		return false;
	}

	BFinalAssetContent Content;

	BFileAssetCreator AssetCreator(&Content, GameThreadTaskHandlers.TaskQueuer);
	BFileAssetCreator* AssetCreatorPtr = &AssetCreator;

	FBFileFactoryInputOption* WithOptionPtr = &WithOption;

	FThreadSafeCounter* UncompletedTasksCount = new FThreadSafeCounter(3); //3 file types

	Async_FactoryCreateBFileContent_ForFileType(
		EBNodeType::EBNodeType_Hierarchy,
		WithOption.HierarchyFileStream,
		WithOption.CompressionState,
		AssetCreatorPtr,
		UncompletedTasksCount);

	Async_FactoryCreateBFileContent_ForFileType(
		EBNodeType::EBNodeType_Geometry,
		WithOption.GeometryFileStream,
		WithOption.CompressionState,
		AssetCreatorPtr,
		UncompletedTasksCount);

	Async_FactoryCreateBFileContent_ForFileType(
		EBNodeType::EBNodeType_Metadata,
		WithOption.MetadataFileStream,
		WithOption.CompressionState,
		AssetCreatorPtr,
		UncompletedTasksCount);

	while (!AssetCreator.IsCompleted() || UncompletedTasksCount->GetValue() > 0)
	{
		GameThreadTaskHandlers.TasksDequeuerExecuter();

		if (UncompletedTasksCount->GetValue() > 0)
			FPlatformProcess::Sleep(0.1f);
	}

	delete UncompletedTasksCount;

	return FinalizeFactoryCreateBFileContent(Result, &Content);
}

bool FBFileAssetFactory::FinalizeFactoryCreateBFileContent(FBFileFactoryOutputOption& Result, BFinalAssetContent* Content)
{
	bool* bSucceedPtr = new bool(true);

	FThreadSafeCounter* UncompletedTasksCount = new FThreadSafeCounter(Result.OutputFiles.Num());
	FEvent* CompletedEvent = FGenericPlatformProcess::GetSynchEventFromPool();

	for (auto& Pair : Result.OutputFiles)
	{
		EBFileOutputFormat OutputFormat = Pair.Key;

		auto OutputBufferAlternative = Pair.Value;

		FBLambdaRunnable::RunLambdaOnDedicatedBackgroundThread([bSucceedPtr, OutputFormat, OutputBufferAlternative, Content, UncompletedTasksCount, CompletedEvent]()
			{
				if (*bSucceedPtr == false)
				{
					if (UncompletedTasksCount->Decrement() == 0) CompletedEvent->Trigger();
					return;
				}

				bool bSuccess = Content->XSerialize(OutputFormat, OutputBufferAlternative);
				if (!bSuccess)
				{
					*bSucceedPtr = false;
				}

				if (UncompletedTasksCount->Decrement() == 0) CompletedEvent->Trigger();
			});
	}

	CompletedEvent->Wait();
	FGenericPlatformProcess::ReturnSynchEventToPool(CompletedEvent);
	delete UncompletedTasksCount;

	bool bSucceed = *bSucceedPtr;
	delete bSucceedPtr;
	return bSucceed;
}

void FBFileAssetFactory::Async_FactoryCreateBFileContent_ForFileType(
	EBNodeType InFileType,
	std::istream* InStream,
	EBFileCompressionState InCompressionState,
	BFileAssetCreator* AssetCreatorPtr,
	FThreadSafeCounter* UncompletedTasksCount)
{
	FBLambdaRunnable::RunLambdaOnDedicatedBackgroundThread([InFileType, InStream, InCompressionState, AssetCreatorPtr, UncompletedTasksCount]()
		{
			BFileReader Reader(InFileType);
			Reader.ReadFromStream(InStream, InCompressionState,
				[](uint32 FileSDKVersion)
				{
				},
				[AssetCreatorPtr](const BHierarchyNode& Node)
				{
					AssetCreatorPtr->ProvideNewNode(Node);
				},
				[AssetCreatorPtr](const BGeometryNode& Node)
				{
					AssetCreatorPtr->ProvideNewNode(Node);
				},
				[AssetCreatorPtr](const BMetadataNode& Node)
				{
					AssetCreatorPtr->ProvideNewNode(Node);
				},
				[](int32 ErrorCode, const FString& ErrorMessage)
				{
					UE_LOG(LogTemp, Error, TEXT("BFileReader->Error: %s"), *ErrorMessage);
				});

			UncompletedTasksCount->Decrement();
		});
}

UBFileAssetFactory::UBFileAssetFactory(const FObjectInitializer& ObjectInitializer) : UFactory(ObjectInitializer)
{
	Formats.Add(FString(TEXT("x3_p;")) + NSLOCTEXT("UBFileAssetFactory", "XFormatPlain", "B Plain Files").ToString());
	Formats.Add(FString(TEXT("x3_c;")) + NSLOCTEXT("UBFileAssetFactory", "XFormatCompressed", "B Compressed Files").ToString());

	SupportedClass = UBFileAsset::StaticClass();

	bCreateNew = false;
	bEditorImport = true;
}

UObject* UBFileAssetFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	FString FileFullPath = FPaths::ConvertRelativePathToFull(Filename);

	UBFileAsset* AssetPtr = NewObject<UBFileAsset>(InParent, InClass, InName, Flags);

	if (FactoryCreateFile_InGameThread(AssetPtr->SerializedContent, FileFullPath))
	{
		bOutOperationCanceled = false;
	}
	else
	{
		bOutOperationCanceled = true;
		return nullptr;
	}

	return AssetPtr;
}

bool UBFileAssetFactory::FactoryCreateFile_InGameThread(TArray<uint8>& DestSerializedContent, const FString& FileFullPath)
{
	//Must be in game-thread
	check(IsInGameThread());

	auto DestSerializedContentPtr = &DestSerializedContent;

	FBFileFactoryOutputOption OutputOption;
	OutputOption.OutputFiles.Add(EBFileOutputFormat::HGM, [DestSerializedContentPtr](int64 IgnoreThis)
		{
			return FBFileOutputBufferAlternative(DestSerializedContentPtr);
		});

	FBFileFactoryGameThreadHandlers Handlers;
	FactoryCreateFile_GameThreadHandlers_Initialize(Handlers);

	return Processor.FactoryCreateBFileContent(OutputOption, FileFullPath, Handlers);
}

bool UBFileAssetFactory::FactoryCreateFile_InGameThread(FBFileFactoryOutputOption& Result, FBFileFactoryInputOption& WithOption)
{
	//Must be in game-thread
	check(IsInGameThread());

	FBFileFactoryGameThreadHandlers Handlers;
	FactoryCreateFile_GameThreadHandlers_Initialize(Handlers);

	return Processor.FactoryCreateBFileContent(Result, WithOption, Handlers);
}

void UBFileAssetFactory::FactoryCreateFile_GameThreadHandlers_Initialize(FBFileFactoryGameThreadHandlers& Handlers)
{
	Handlers.TaskQueuer = [this](TFunction<void()> NewTask)
	{
		AddGameThreadTask(NewTask);
	};
	Handlers.TasksDequeuerExecuter = [this]()
	{
		while (AwaitingGameThreadTasks.Num() > 0)
		{
			TFunction<void()> AwaitingTask;
			{
				FScopeLock Lock(&AwaitingGameThreadTasks_Mutex);
				AwaitingTask = AwaitingGameThreadTasks[0];
			}
			AwaitingGameThreadTasks.RemoveAt(0);

			AwaitingTask();
		}
	};
}

void UBFileAssetFactory::AddGameThreadTask(TFunction<void()> InTask)
{
	FScopeLock Lock(&AwaitingGameThreadTasks_Mutex);
	AwaitingGameThreadTasks.Add(InTask);
}