#!/bin/bash

echo "Running Process Unreal Optimizer Commandlet with a parameter: $1"

/home/ue4/UnrealEngine/Engine/Binaries/Linux/UE4Editor-Cmd "/tmp/project/Project.uproject" -run=ProcessUnrealOptimizer $1