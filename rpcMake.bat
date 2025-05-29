@echo off
set PROTOC_PATH=d:\server_and_environment\newgrpc\grpc\visualpro\third_party\protobuf\Debug\protoc.exe
set GRPC_PLUGIN_PATH=D:\server_and_environment\newgrpc\grpc\visualpro\Debug\grpc_cpp_plugin.exe
set PROTO_FILE=message.proto

echo Generating gRPC code from %PROTO_FILE%...
"%PROTOC_PATH%" -I=. --grpc_out=. --plugin=protoc-gen-grpc="%GRPC_PLUGIN_PATH%" "%PROTO_FILE%"

echo Generating Protobuf c++ code from %PROTO_FILE%...
"%PROTOC_PATH%" -I=. --cpp_out=. "%PROTO_FILE%"

echo Done.
pause