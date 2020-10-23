/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#include "ProcessUnrealOptimizerCommandlet.h"
#include "BFileSDKCommandlet.h"
#include "Misc/FileHelper.h"
#include "BFileFinalTypes.h"
#include "BFileHeader.h"
#include "BHttpClient.h"
#include "BLambdaRunnable.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include <fstream>

UProcessUnrealOptimizerCommandlet::UProcessUnrealOptimizerCommandlet()
{
	IsClient = false;
	IsEditor = false;
	IsServer = false;
	LogToConsole = true;
}

int32 UProcessUnrealOptimizerCommandlet::Main(const FString& Params)
{
	UE_LOG(LogCommandletPlugin, Display, TEXT("************************** Process Unreal Optimizer Commandlet has started **************************"));
	UE_LOG(LogCommandletPlugin, Display, TEXT("Passed arguments: %s"), *Params);

    //We expect last arg to be CadProcessParameterRequestUrl
	FString ArgsString = Params.TrimStartAndEnd();
	TArray<FString> Args;
    ArgsString.ParseIntoArray(Args, TEXT(" "), true);
    if (Args.Num() > 0)
    {
        CadProcessParameterRequestUrl = Args[Args.Num() - 1];
    }
    else
    {
        CadProcessParameterRequestUrl = ArgsString;
    }
	CadProcessParameterRequestUrl = CadProcessParameterRequestUrl.Replace(TEXT("\""), TEXT(""));
	CadProcessParameterRequestUrl = CadProcessParameterRequestUrl.Replace(TEXT("'"), TEXT(""));

    UE_LOG(LogCommandletPlugin, Display, TEXT("CadProcessParameterRequestUrl: %s"), *CadProcessParameterRequestUrl);

    int32 ParameterRequestReturnCode;
    if (!GetCADProcessParameters(ParameterRequestReturnCode))
    {
		UE_LOG(LogCommandletPlugin, Error, TEXT("Parameters could not be fetched from CAD Process service. Return code: %d"), ParameterRequestReturnCode);
        return 1;
    }

    XCHQueueParameter = FBQueueStreamParameter::MakeInitialized();
    XCGQueueParameter = FBQueueStreamParameter::MakeInitialized();
    XCMQueueParameter = FBQueueStreamParameter::MakeInitialized();

    FBLambdaRunnable::RunLambdaOnDedicatedBackgroundThread([this]()
        {
			FBOQueueStream OStreamToDownload_For_XCH_File(XCHQueueParameter);
			int32 ResponseCode = BHttpClient::Get(&OStreamToDownload_For_XCH_File, XCHDownloadUrl);
            if (ResponseCode >= 400 || ResponseCode < 200)
            {
                HttpRequests_FatalError_State.Increment();
                HttpRequests_FatalError_Messages.Enqueue(FString::Printf(TEXT("HCF Download Stage: %s => %d"), *XCHDownloadUrl, ResponseCode));
            }
        });

    FBLambdaRunnable::RunLambdaOnDedicatedBackgroundThread([this]()
        {
			FBOQueueStream OStreamToDownload_For_XCG_File(XCGQueueParameter);
			int32 ResponseCode = BHttpClient::Get(&OStreamToDownload_For_XCG_File, XCGDownloadUrl);
            if (ResponseCode >= 400 || ResponseCode < 200)
            {
				HttpRequests_FatalError_State.Increment();
				HttpRequests_FatalError_Messages.Enqueue(FString::Printf(TEXT("GCF Download Stage: %s => %d"), *XCGDownloadUrl, ResponseCode));
            }
        });

    FBLambdaRunnable::RunLambdaOnDedicatedBackgroundThread([this]()
        {
            FBOQueueStream OStreamToDownload_For_XCM_File(XCMQueueParameter);
            int32 ResponseCode = BHttpClient::Get(&OStreamToDownload_For_XCM_File, XCMDownloadUrl);
            if (ResponseCode >= 400 || ResponseCode < 200)
            {
				HttpRequests_FatalError_State.Increment();
				HttpRequests_FatalError_Messages.Enqueue(FString::Printf(TEXT("MCF Download Stage: %s => %d"), *XCMDownloadUrl, ResponseCode));
            }
        });

    FBIQueueStream IStreamToDownload_For_XCH_File(XCHQueueParameter);
    FBIQueueStream IStreamToDownload_For_XCG_File(XCGQueueParameter);
    FBIQueueStream IStreamToDownload_For_XCM_File(XCMQueueParameter);

    FBFileFactoryInputOption InputOption;
    InputOption.CompressionState = EBFileCompressionState::Compressed;
    InputOption.HierarchyFileStream = &IStreamToDownload_For_XCH_File;
    InputOption.GeometryFileStream = &IStreamToDownload_For_XCG_File;
    InputOption.MetadataFileStream = &IStreamToDownload_For_XCM_File;

    FBFileFactoryOutputOption OutputOption;
    AddOutputFileProcessor(OutputOption, EBFileOutputFormat::HGM);
    AddOutputFileProcessor(OutputOption, EBFileOutputFormat::HG);
	AddOutputFileProcessor(OutputOption, EBFileOutputFormat::H);
	AddOutputFileProcessor(OutputOption, EBFileOutputFormat::Gs);

    AssetFactory = NewObject<UBFileAssetFactory>();
    bool bResult = AssetFactory->FactoryCreateFile_InGameThread(OutputOption, InputOption);
    AssetFactory = nullptr;

    if (HttpRequests_FatalError_State.GetValue() > 0)
    {
        bResult = false;

        FString ErrorMessage;
        while (HttpRequests_FatalError_Messages.Dequeue(ErrorMessage))
        {
            UE_LOG(LogCommandletPlugin, Error, TEXT("Error: %s"), *ErrorMessage);
        }
    }

    if (!bResult)
    {
        UE_LOG(LogCommandletPlugin, Display, TEXT("************************** Process Unreal Optimizer Commandlet has failed to complete the task **************************"));
        return 1;
    }
    UE_LOG(LogCommandletPlugin, Display, TEXT("************************** Process Unreal Optimizer Commandlet has completed the task successfully **************************"));
    return 0;
}

void UProcessUnrealOptimizerCommandlet::AddOutputFileProcessor(FBFileFactoryOutputOption& OutputOption, EBFileOutputFormat OutputFormat)
{
	auto Function = [this, OutputFormat](int64 GeometryUniqueID /*if OutputFormat == GS else ignore*/)
	{
		auto QueueParameter = FBQueueStreamParameter::MakeInitialized();

        FString RequestUploadUrl_PostUrlParameters;

        if (OutputFormat == EBFileOutputFormat::Gs)
        {
            RequestUploadUrl_PostUrlParameters = FString::Printf(TEXT("?fileType=gs&fileName=%lld"), GeometryUniqueID);
        }
        else if (OutputFormat == EBFileOutputFormat::HGM)
        {
            RequestUploadUrl_PostUrlParameters = "?fileType=hgm";
        }
		else if (OutputFormat == EBFileOutputFormat::HG)
		{
			RequestUploadUrl_PostUrlParameters = "?fileType=hg";
		}
		else //H
		{
			RequestUploadUrl_PostUrlParameters = "?fileType=h";
		}

        FBFileOutputBufferAlternative StreamWrapper;
        StartUploadProcess(StreamWrapper, RequestUploadUrl_PostUrlParameters, QueueParameter, OutputFormat); //No need to handle true-false; since the inner stream is null; it will be handled automatically. FactoryCreateBFileContent will return false.
        return StreamWrapper;
	};

    OutputOption.OutputFiles.Add(OutputFormat, Function);
}

 bool UProcessUnrealOptimizerCommandlet::StartUploadProcess(
    FBFileOutputBufferAlternative& Result, 
    const FString& RequestUploadUrl_PostUrlParameters, 
	const FBQueueStreamParameter& QueueParameter,
	EBFileOutputFormat OutputFormat)
 {
    FString UploadUrl;
    int32 ResponseCode = GetUploadRequestUrl((CadProcessUploadRequestUrl + RequestUploadUrl_PostUrlParameters), UploadUrl);
    if (ResponseCode >= 400 || ResponseCode < 200)
	{
		HttpRequests_FatalError_State.Increment();
		HttpRequests_FatalError_Messages.Enqueue(FString::Printf(TEXT("Upload Stage - %u: %s => %d"), (uint8)OutputFormat, *UploadUrl, ResponseCode));
        return false;
    }

	auto PtrToIStreamForUpload_For_File = new FBIQueueStream(QueueParameter);
    auto PtrToOStreamForProcess_For_File = new FBOQueueStream(QueueParameter);

    FBLambdaRunnable::RunLambdaOnDedicatedBackgroundThread([this, PtrToIStreamForUpload_For_File, UploadUrl, OutputFormat]()
        {
            int32 ResponseCode = BHttpClient::Put(PtrToIStreamForUpload_For_File, nullptr, UploadUrl, FString(TEXT("application/octet-stream")));
            if (ResponseCode >= 400 || ResponseCode < 200)
            {
				HttpRequests_FatalError_State.Increment();
				HttpRequests_FatalError_Messages.Enqueue(FString::Printf(TEXT("Upload Stage - %u: %s => %d"), (uint8)OutputFormat, *UploadUrl, ResponseCode));
            }
            delete PtrToIStreamForUpload_For_File;
        });

    Result = FBFileOutputBufferAlternative(PtrToOStreamForProcess_For_File, [PtrToOStreamForProcess_For_File]
    {
        delete PtrToOStreamForProcess_For_File;
    });
    return true;
}

int32 UProcessUnrealOptimizerCommandlet::GetUploadRequestUrl(const FString& Url, FString& UploadRequestUrl)
{
    std::ostringstream ResultStream;
   
	int32 ResponseCode = BHttpClient::Get(&ResultStream, Url);
	FString ResultBody = FString(ANSI_TO_TCHAR(ResultStream.str().c_str()));

	if (ResponseCode >= 400 || ResponseCode < 200)
	{
		UE_LOG(LogCommandletPlugin, Error, TEXT("Process Unreal Optimizer Commandlet GetUploadRequestUrl: Response: %d => %s"), ResponseCode, *ResultBody);
	}
	else
	{
		TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ResultBody), JsonObject))
		{
			UE_LOG(LogCommandletPlugin, Error, TEXT("Process Unreal Optimizer Commandlet GetUploadRequestUrl: Response is not a valid json object. Response: %d => %s"), ResponseCode, *ResultBody);
            ResponseCode = 999;
		}
		else if (!JsonObject->HasTypedField<EJson::String>("uploadUrl"))
		{
			UE_LOG(LogCommandletPlugin, Error, TEXT("Process Unreal Optimizer Commandlet GetUploadRequestUrl: Response does not contain uploadUrl field. Response: %d => %s"), ResponseCode, *ResultBody);
            ResponseCode = 999;
		}
		else
		{
			UploadRequestUrl = JsonObject->GetStringField("uploadUrl");
		}
	}
    return ResponseCode;
}

bool UProcessUnrealOptimizerCommandlet::GetCADProcessParameters(int32& ParameterRequestReturnCode)
{
	if (CadProcessParameterRequestUrl.Len() == 0)
	{
		UE_LOG(LogCommandletPlugin, Error, TEXT("CAD_PROCESS_PARAMETER_REQUEST_URL environment variable is undefined."));
		return false;
	}

	do
	{
		std::ostringstream ResultStream;

        ParameterRequestReturnCode = BHttpClient::Get(&ResultStream, CadProcessParameterRequestUrl);
		FString ResultBody = FString(ANSI_TO_TCHAR(ResultStream.str().c_str()));

        if (ParameterRequestReturnCode == 404) continue;
		else if (ParameterRequestReturnCode >= 400 || ParameterRequestReturnCode < 200)
		{
			UE_LOG(LogCommandletPlugin, Error, TEXT("Process Unreal Optimizer Commandlet GetCADProcessParameters: Response: %d => %s"), ParameterRequestReturnCode, *ResultBody);
            return false;
		}
		else
		{
			TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
			if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ResultBody), JsonObject))
			{
				UE_LOG(LogCommandletPlugin, Error, TEXT("Process Unreal Optimizer Commandlet GetCADProcessParameters: Response is not a valid json object. Response: %d => %s"), ParameterRequestReturnCode, *ResultBody);
                ParameterRequestReturnCode = 999;
                return false;
			}
			else if (!JsonObject->HasTypedField<EJson::String>("uploadRequestUrl")
                || !JsonObject->HasTypedField<EJson::String>("downloadHierarchyCfUrl")
                || !JsonObject->HasTypedField<EJson::String>("downloadGeometryCfUrl")
                || !JsonObject->HasTypedField<EJson::String>("downloadMetadataCfUrl"))
			{
				UE_LOG(LogCommandletPlugin, Error, TEXT("Process Unreal Optimizer Commandlet GetCADProcessParameters: Response does not contain necessary fields. Response: %d => %s"), ParameterRequestReturnCode, *ResultBody);
                ParameterRequestReturnCode = 999;
                return false;
			}

			CadProcessUploadRequestUrl = JsonObject->GetStringField("uploadRequestUrl");
			XCHDownloadUrl = JsonObject->GetStringField("downloadHierarchyCfUrl");
			XCGDownloadUrl = JsonObject->GetStringField("downloadGeometryCfUrl");
			XCMDownloadUrl = JsonObject->GetStringField("downloadMetadataCfUrl");

            return true;
		}

	} while (Sleep_Internal(10));

    return false;

}

bool UProcessUnrealOptimizerCommandlet::Sleep_Internal(float Seconds)
{
	FPlatformProcess::Sleep(Seconds);
	return true;
}