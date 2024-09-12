SET PluginPath="D:\OneDrive\UnrealEnginePlugins\PhysicalSImulation\CustomRenderFeature.uplugin"
SET PackagePath="D:\OneDrive\UnrealEnginePlugins\Package\CustomRenderFeature5_2"
D:
cd "D:\Epic Games\UE_5.3\Engine\Build\BatchFiles"
call RunUAT.bat BuildPlugin -Plugin=%PluginPath% -Package=%PackagePath% -Rocket
cd %PackagePath%
7z.exe a -r CustomRenderFeature5_3.zip
MOVE CustomRenderFeature5_3.zip ..\
