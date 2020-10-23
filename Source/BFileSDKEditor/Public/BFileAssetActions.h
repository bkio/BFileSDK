/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"
#include "Templates/SharedPointer.h"

class BFILESDKEDITOR_API FBFileAssetActions : public FAssetTypeActions_Base
{
public:

	/**
	 * Creates and initializes a new instance.
	 */
	FBFileAssetActions();

public:

	//~ FAssetTypeActions_Base overrides

	virtual bool CanFilter() override;
	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override;
	virtual uint32 GetCategories() override;
	virtual FText GetName() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual FColor GetTypeColor() const override;
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override;
};