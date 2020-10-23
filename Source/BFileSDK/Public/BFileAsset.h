/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BFileAsset.generated.h"

UCLASS(BlueprintType)
class BFILESDK_API UBFileAsset : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BFileSDK")
	TArray<uint8> SerializedContent;

	TSharedPtr<class BFinalAssetContent> DeserializedContent;
};