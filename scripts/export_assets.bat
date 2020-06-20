@echo off

blender n:/assets/rig.blend --background --python ./scripts/blender_export.py

for %%G in ("n:\assets\*.nnm") do (
  mklink "n:\src\%%~nxG" "%%~G" 2> NUL
)
