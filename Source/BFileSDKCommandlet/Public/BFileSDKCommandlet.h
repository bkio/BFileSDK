/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#pragma once

#include "Logging/LogMacros.h"

/** Declares a log category for this module. */
DECLARE_LOG_CATEGORY_EXTERN(LogCommandletPlugin, Log, All);

/**
 * Implements the CommandletPlugin module.
 */
class FBFileSDKCommandletModule
	: public IModuleInterface
{
public:

	//~ IModuleInterface interface

	virtual void StartupModule() override;
	virtual void ShutdownModule() {}

	virtual bool SupportsDynamicReloading() override
	{
		return true;
	}
};