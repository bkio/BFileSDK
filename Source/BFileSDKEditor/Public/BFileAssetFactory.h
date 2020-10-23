/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include <fstream>
#include "BFileAssetFactory.generated.h"

enum EBFileCompressionState : uint8;
enum EBNodeType : uint8;
enum EBFileOutputFormat : uint8;

/*
 * Input struct
 */
struct BFILESDKEDITOR_API FBFileFactoryInputOption
{

public:
	EBFileCompressionState CompressionState;

	std::istream* HierarchyFileStream;
	std::istream* GeometryFileStream;
	std::istream* MetadataFileStream;

	FBFileFactoryInputOption(
		EBFileCompressionState InCompressionState,
		std::istream* WithHierarchyFileStream,
		std::istream* WithGeometryFileStream,
		std::istream* WithMetadataFileStream)
	{
		CompressionState = InCompressionState;

		HierarchyFileStream = WithHierarchyFileStream;
		GeometryFileStream = WithGeometryFileStream;
		MetadataFileStream = WithMetadataFileStream;
	}

	FBFileFactoryInputOption() {}
};

/*
 * Output struct: EBFileOutputFormat -> std::ostream* generator
 */
struct BFILESDKEDITOR_API FBFileFactoryOutputOption
{

public:
	TMap<EBFileOutputFormat, TFunction<struct FBFileOutputBufferAlternative(int64)>> OutputFiles;

	FBFileFactoryOutputOption(
		const TMap<EBFileOutputFormat, TFunction<struct FBFileOutputBufferAlternative(int64)>>& InOutputFiles)
	{
		OutputFiles = InOutputFiles;
	}

	FBFileFactoryOutputOption() {}
};

/*
 * Game-thread handlers
 */
struct BFILESDKEDITOR_API FBFileFactoryGameThreadHandlers
{

public:
	TFunction<void(TFunction<void()>)> TaskQueuer;
	TFunction<void()> TasksDequeuerExecuter;

	FBFileFactoryGameThreadHandlers(
		TFunction<void(TFunction<void()>)> InTaskQueuer,
		TFunction<void()> InTasksDequeuerExecuter);

private:
	friend class FBFileAssetFactory;
	friend class UBFileAssetFactory;

	//Auto handle
	FBFileFactoryGameThreadHandlers();

	FThreadSafeCounter OptionalsNotProvided_AwaitingGameThreadTasks_Num;
};

/*
* Non-UObject based asset factory
*/
class BFILESDKEDITOR_API FBFileAssetFactory
{

public:
	bool FactoryCreateBFileContent(
		FBFileFactoryOutputOption& Result,
		const FString& FullFilePath_ToZip, 
		FBFileFactoryGameThreadHandlers& GameThreadTaskHandlers);

	bool FactoryCreateBFileContent(
		FBFileFactoryOutputOption& Result,
		FBFileFactoryInputOption& WithOption);

	bool FactoryCreateBFileContent(
		FBFileFactoryOutputOption& Result,
		FBFileFactoryInputOption& WithOption,
		FBFileFactoryGameThreadHandlers& GameThreadTaskHandlers);

private:
	void Async_FactoryCreateBFileContent_ForFileType(
		EBNodeType InFileType,
		std::istream* InStream,
		EBFileCompressionState InCompressionState,
		class BFileAssetCreator* AssetCreatorPtr,
		FThreadSafeCounter* UncompletedTasksCount);

	bool FinalizeFactoryCreateBFileContent(FBFileFactoryOutputOption& Result, class BFinalAssetContent* Content);
};

/*
* UObject based asset factory
*/
UCLASS()
class BFILESDKEDITOR_API UBFileAssetFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

public:
	void AddGameThreadTask(TFunction<void()> InTask);

	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;

	bool FactoryCreateFile_InGameThread(TArray<uint8>& DestSerializedContent, const FString& FileFullPath);
	bool FactoryCreateFile_InGameThread(FBFileFactoryOutputOption& Result, FBFileFactoryInputOption& WithOption);

private:
	FBFileAssetFactory Processor;

	void FactoryCreateFile_GameThreadHandlers_Initialize(FBFileFactoryGameThreadHandlers& Handlers);

	FCriticalSection AwaitingGameThreadTasks_Mutex;
	TArray<TFunction<void()>> AwaitingGameThreadTasks;
};