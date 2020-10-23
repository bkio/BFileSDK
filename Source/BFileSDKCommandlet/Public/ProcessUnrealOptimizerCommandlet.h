/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#pragma once

#include "Commandlets/Commandlet.h"
#include "Runtime/Online/HTTP/Public/Http.h"
#include "BQueueStream.h"
#include "BFileAssetFactory.h"
#include "ProcessUnrealOptimizerCommandlet.generated.h"

UCLASS()
class UProcessUnrealOptimizerCommandlet
	: public UCommandlet
{
	GENERATED_BODY()

public:
	/** Default constructor. */
	UProcessUnrealOptimizerCommandlet();

	//~ UCommandlet interface
	virtual int32 Main(const FString& Params) override;

private:
	int32 GetUploadRequestUrl(const FString& Url, FString& UploadRequestUrl);
    bool StartUploadProcess(
        FBFileOutputBufferAlternative& Result, 
        const FString& RequestUploadUrl_PostUrlParameters, 
        const FBQueueStreamParameter& QueueParameter,
        EBFileOutputFormat OutputFormat);

    void AddOutputFileProcessor(FBFileFactoryOutputOption& OutputOption, EBFileOutputFormat OutputFormat);

    //Entry point
    FString CadProcessParameterRequestUrl;

    FString CadProcessUploadRequestUrl;
    FString XCHDownloadUrl;
    FString XCGDownloadUrl;
    FString XCMDownloadUrl;

    FBQueueStreamParameter XCHQueueParameter;
    FBQueueStreamParameter XCGQueueParameter;
    FBQueueStreamParameter XCMQueueParameter;

    FThreadSafeCounter HttpRequests_FatalError_State;
    TQueue<FString, EQueueMode::Mpsc> HttpRequests_FatalError_Messages;

    bool GetCADProcessParameters(int32& ParameterRequestReturnCode);
    bool Sleep_Internal(float Seconds);

    UPROPERTY()
    class UBFileAssetFactory* AssetFactory; //To prevent GC
};