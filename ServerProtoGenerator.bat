@echo off
cd /d "%~dp0"
start protoc.exe --cpp_out=. ShooterProtocol.proto