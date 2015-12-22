/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.0x                             */
/*               Copyright (C)2015, WWIV Software Services                */
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
#ifndef __INCLUDED_SDK_MESSAGE_WWIV_H__
#define __INCLUDED_SDK_MESSAGE_WWIV_H__

#include <string>

#include "sdk/msgapi/msgapi.h"
#include "sdk/vardec.h"

namespace wwiv {
namespace sdk {
namespace msgapi {

class WWIVMessageHeader: public MessageHeader {
public:
  explicit WWIVMessageHeader(postrec header);
  virtual ~WWIVMessageHeader();

  virtual std::string title() override { return header_.title;  }
  virtual std::string to() override { return "ALL";  }
  virtual std::string from() override;
  virtual uint32_t daten() override { return header_.daten; }
  virtual uint8_t status() override;
  virtual uint8_t anony() override;
  virtual std::string oaddress() override;

  virtual std::string destination_address() override { return "";  }

  virtual bool is_local() override; // ??
  virtual bool is_private() override { return false;  }
  virtual bool is_locked() override { return false; }
  virtual bool is_deleted() override { return (header_.status & status_delete) != 0; }

private:
  postrec header_;
};

class WWIVMessageText: public MessageText {
public:
  WWIVMessageText();
private:
  std::string text_;
};

class WWIVMessage: public Message {
public:
  WWIVMessage(MessageHeader* header, MessageText* text);
  ~WWIVMessage();

private:
  WWIVMessageHeader header_;
  WWIVMessageText text_;
};

}  // namespace msgapi
}  // namespace sdk
}  // namespace wwiv


#endif  // __INCLUDED_SDK_MESSAGE_WWIV_H__
