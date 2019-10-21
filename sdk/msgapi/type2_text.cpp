/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2019, WWIV Software Services             */
/*                                                                        */
/*    Licensed  under the  Apache License, Version  2.0 (the "License");  */
/*    you may not use this  file  except in compliance with the License.  */
/*    You may obtain a copy of the License at                             */
/*                                                                        */
/*                http://www.apache.org/licenses/LICENSE-2.0              */
/*                                                                        */
/*    Unless  required  by  applicable  law  or agreed to  in  writing,   */
/*    software  distributed  under  the  License  is  distributed on an   */
/*    "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/**************************************************************************/
#include "sdk/msgapi/type2_text.h"

#include "core/datafile.h"
#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "sdk/vardec.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace wwiv::sdk::msgapi {

using std::string;
using std::make_unique;
using std::unique_ptr;
using std::vector;
using wwiv::core::DataFile;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

constexpr char CZ = 26;

Type2Text::Type2Text(std::filesystem::path p) : path_(std::move(p)) {}

// Implementation Details

bool Type2Text::remove_link(messagerec& msg) {
  auto file = OpenMessageFile();
  if (!file || !file->IsOpen()) {
    return false;
  }
  const size_t section = static_cast<int>(msg.stored_as / GAT_NUMBER_ELEMENTS);
  auto gat = load_gat(*file, section);
  uint32_t current_section = msg.stored_as % GAT_NUMBER_ELEMENTS;
  while (current_section > 0 && current_section < GAT_NUMBER_ELEMENTS) {
    const uint32_t next_section = static_cast<long>(gat[current_section]);
    gat[current_section] = 0;
    current_section = next_section;
  }
  save_gat(*file, section, gat);
  file->Close();
  return true;
}


/**
* Opens the message area file {messageAreaFileName} and returns the file handle.
* Note: This is a Private method to this module.
*/
std::optional<wwiv::core::File> Type2Text::OpenMessageFile() const {
  // TODO(rushfan): Pass in the status manager. this is needed to
  // set a()->subchg if any of the subs receive a post so that 
  // resynch can work right on multi node configs.
  // 
  // a()->status_manager()->RefreshStatusCache();

  File message_file(path_);
  if (!message_file.Open(File::modeReadWrite | File::modeBinary)) {
    // TODO(rushfan): Switch to std::optional?
    return std::nullopt;
  }
  return message_file;
}

std::vector<gati_t> Type2Text::load_gat(File& file, size_t section) {
  std::vector<gati_t> gat(GAT_NUMBER_ELEMENTS);
  auto file_size = file.length();
  auto section_pos = section * GATSECLEN;
  if (file_size < static_cast<long>(section_pos)) {
    file.set_length(section_pos);
    file_size = section_pos;
  }
  file.Seek(section_pos, File::Whence::begin);
  if (file_size < section_pos + GAT_SECTION_SIZE) {
    // TODO(rushfan): Check that gat is loaded.
    file.Write(&gat[0], GAT_SECTION_SIZE);
  } else {
    // TODO(rushfan): Check that gat is loaded.
    file.Read(&gat[0], GAT_SECTION_SIZE);
  }
  return gat;
}

void Type2Text::save_gat(File& file, size_t section, const std::vector<gati_t>& gat) {
  auto section_pos = section * GATSECLEN;
  file.Seek(section_pos, File::Whence::begin);
  file.Write(&gat[0], GAT_SECTION_SIZE);

  // TODO(rushfan): Pass in the status manager. this is needed to
  // set a()->subchg if any of the subs receive a post so that 
  // resynch can work right on multi node configs.
  // 
  // auto status = a()->status_manager()->BeginTransaction();
  // status->IncrementFileChangedFlag(WStatus::fileChangePosts);
  // a()->status_manager()->CommitTransaction(status);
}

bool Type2Text::readfile(const messagerec* msg, string* out) {
  out->clear();
  auto file(OpenMessageFile());
  if (!file || !file->IsOpen()) {
    // TODO(rushfan): set error code,
    return false;
  }
  const uint32_t gat_section = msg->stored_as / GAT_NUMBER_ELEMENTS;
  auto gat = load_gat(*file, gat_section);

  auto current_section = msg->stored_as % GAT_NUMBER_ELEMENTS;
  while (current_section > 0 && current_section < GAT_NUMBER_ELEMENTS) {
    file->Seek(MSG_STARTING(gat_section) + MSG_BLOCK_SIZE * current_section, File::Whence::begin);
    char b[MSG_BLOCK_SIZE + 1];
    file->Read(b, MSG_BLOCK_SIZE);
    b[MSG_BLOCK_SIZE] = 0;
    out->append(b);
    current_section = gat[current_section];
  }

  const auto last_cz = out->find_last_of(CZ);
  const auto last_block_start = out->length() - MSG_BLOCK_SIZE;
  if (last_cz != string::npos && last_block_start >= 0 && last_cz > last_block_start) {
    // last block has a Control-Z in it.  Make sure we add a 0 after it.
    out->resize(last_cz);
  }
  return true;
}

bool Type2Text::savefile(const string& text, messagerec* msg) {
  vector<gati_t> gati;
  auto msgfile(OpenMessageFile());
  if (!msgfile || !msgfile->IsOpen()) {
    // Unable to write to the message file.
    msg->stored_as = 0xffffffff;
    return false;
  }
  size_t section;
  for (section = 0; section < 1024; section++) {
    auto gat = load_gat(*msgfile, section);
    const auto nNumBlocksRequired = static_cast<int>((text.length() + 511L) / MSG_BLOCK_SIZE);
    gati_t i4 = 1;
    gati.clear();
    while (size_int(gati) < nNumBlocksRequired && i4 < GAT_NUMBER_ELEMENTS) {
      if (gat[i4] == 0) {
        gati.push_back(i4);
      }
      ++i4;
    }
    if (size_int(gati) >= nNumBlocksRequired) {
      gati.push_back(-1);
      for (auto i = 0; i < nNumBlocksRequired; i++) {
        msgfile->Seek(MSG_STARTING(section) + MSG_BLOCK_SIZE * static_cast<long>(gati[i]), File::Whence::begin);
        msgfile->Write((&text[i * MSG_BLOCK_SIZE]), MSG_BLOCK_SIZE);
        gat[gati[i]] = gati[i + 1];
      }
      save_gat(*msgfile, section, gat);
      break;
    }
  }
  msg->stored_as = static_cast<uint32_t>(gati[0]) + static_cast<uint32_t>(section) * GAT_NUMBER_ELEMENTS;
  return true;
}

} // namespace wwiv
