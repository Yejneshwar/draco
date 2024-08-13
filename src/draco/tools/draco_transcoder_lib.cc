// Copyright 2021 The Draco Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#include "draco/tools/draco_transcoder_lib.h"

#ifdef DRACO_TRANSCODER_SUPPORTED
#include "draco/core/status_or.h"
#include "draco/io/file_utils.h"
#include "draco/io/scene_io.h"
#include "draco/scene/scene_utils.h"
#include "draco/texture/texture_utils.h"
#include <draco/io/gltf_decoder.h>

#include <fstream>
#include <sstream>

namespace draco {

DracoTranscoder::DracoTranscoder() {}

StatusOr<std::unique_ptr<DracoTranscoder>> DracoTranscoder::Create(
    const DracoTranscodingOptions &options) {
  DRACO_RETURN_IF_ERROR(options.geometry.Check());
  std::unique_ptr<DracoTranscoder> dt(new DracoTranscoder());
  dt->transcoding_options_ = options;
  return dt;
}

StatusOr<std::unique_ptr<DracoTranscoder>> DracoTranscoder::Create(
    const DracoCompressionOptions &options) {
  DracoTranscodingOptions new_options;
  new_options.geometry = options;
  return Create(new_options);
}

Status DracoTranscoder::Transcode(const FileOptions &file_options) {
  DRACO_RETURN_IF_ERROR(ReadScene(file_options));
  DRACO_RETURN_IF_ERROR(CompressScene());
  DRACO_RETURN_IF_ERROR(WriteScene(file_options));
  return OkStatus();
}

template <typename U, typename V>
Status DracoTranscoder::TranscodeStream(U *in_stream, V *out_stream) {
  DRACO_RETURN_IF_ERROR(ReadSceneStream<U>(in_stream));
  DRACO_RETURN_IF_ERROR(CompressScene());
  DRACO_RETURN_IF_ERROR(WriteSceneStream<V>(out_stream));
  return OkStatus();
}

template Status DracoTranscoder::TranscodeStream<std::fstream>(
    std::fstream *in_stream, std::fstream *out_stream);
template Status DracoTranscoder::TranscodeStream<std::stringstream>(
    std::stringstream *in_stream, std::stringstream *out_stream);
template Status DracoTranscoder::TranscodeStream<std::stringstream, std::fstream>(
    std::stringstream *in_stream, std::fstream *out_stream);
template Status DracoTranscoder::TranscodeStream<std::fstream, std::stringstream>(
    std::fstream *in_stream, std::stringstream *out_stream);

Status DracoTranscoder::ReadScene(const FileOptions &file_options) {
  if (file_options.input_filename.empty()) {
    return Status(Status::DRACO_ERROR, "Input filename is empty.");
  } else if (file_options.output_filename.empty()) {
    return Status(Status::DRACO_ERROR, "Output filename is empty.");
  }
  DRACO_ASSIGN_OR_RETURN(scene_,
                         ReadSceneFromFile(file_options.input_filename));
  return OkStatus();
}

template <typename T>
Status DracoTranscoder::ReadSceneStream(T* stream) {

  GltfDecoder decoder;
  DecoderBuffer buffer;

  stream->seekg(0, std::ios::beg);
 
  //get Size of file
  std::streamsize size = stream->tellg();
  stream->seekg(0, std::ios::end);
  std::streamsize end = stream->tellg();
  size = end - size;

   std::vector<char> buf(size);
   stream->seekg(0, std::ios::beg);
   stream->read(&buf[0], size);

   buffer.Init(buf.data(), size);

  DRACO_ASSIGN_OR_RETURN(scene_, decoder.DecodeFromBufferToScene(&buffer));
  return OkStatus();
}

template Status DracoTranscoder::ReadSceneStream<std::fstream>(
    std::fstream *stream);
template Status DracoTranscoder::ReadSceneStream<std::stringstream>(
    std::stringstream *stream);

Status DracoTranscoder::WriteScene(const FileOptions &file_options) {
  if (!file_options.output_bin_filename.empty() &&
      !file_options.output_resource_directory.empty()) {
    DRACO_RETURN_IF_ERROR(gltf_encoder_.EncodeFile<Scene>(
        *scene_, file_options.output_filename, file_options.output_bin_filename,
        file_options.output_resource_directory));
  } else if (!file_options.output_bin_filename.empty()) {
    DRACO_RETURN_IF_ERROR(
        gltf_encoder_.EncodeFile<Scene>(*scene_, file_options.output_filename,
                                        file_options.output_bin_filename));
  } else {
    DRACO_RETURN_IF_ERROR(
        gltf_encoder_.EncodeFile<Scene>(*scene_, file_options.output_filename));
  }
  return OkStatus();
}

template <typename T>
Status DracoTranscoder::WriteSceneStream(T *stream) {
    
  stream->seekg(0, std::ios::beg);
  std::streamsize start = stream->tellg(); 
  stream->seekg(0, std::ios::end);
  std::streamsize end = stream->tellg();

  if (start != end) {
    return Status(Status::IO_ERROR, "Output Stream contains data. Make sure stream has no data.");
  }

  EncoderBuffer buffer;

  DRACO_RETURN_IF_ERROR(
      gltf_encoder_.EncodeToBuffer<Scene>(*scene_, &buffer));

  stream->write(buffer.data(), buffer.size());

  return OkStatus();
}

template Status DracoTranscoder::WriteSceneStream<std::fstream>(
    std::fstream *stream);
template Status DracoTranscoder::WriteSceneStream<std::stringstream>(
    std::stringstream *stream);

Status DracoTranscoder::CompressScene() {
  // Apply geometry compression settings to all scene meshes.
  SceneUtils::SetDracoCompressionOptions(&transcoding_options_.geometry,
                                         scene_.get());
  return OkStatus();
}

}  // namespace draco
#endif  // DRACO_TRANSCODER_SUPPORTED
