#include "gtest/gtest.h"
#include "core_test/file_helper.h"
#include "networkb/binkp.h"
#include "networkb/binkp_commands.h"
#include "networkb/binkp_config.h"
#include "networkb_test/fake_connection.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

using std::string;
using std::thread;
using std::unique_ptr;
using namespace wwiv::net;

static const int ANSWERING_ADDRESS = 1;
static const int ORIGINATING_ADDRESS = 2;


class BinkTest : public testing::Test {
protected:
  void Start() {
    BinkConfig* dummy_config = new BinkConfig(ORIGINATING_ADDRESS, "Dummy", ANSWERING_ADDRESS);
    binkp_.reset(new BinkP(&conn_, dummy_config, BinkSide::ANSWERING, ANSWERING_ADDRESS));
    thread_ = thread([&]() {binkp_->Run(); });
  } 

  void Stop() {
    thread_.join();
  }

  unique_ptr<BinkP> binkp_;
  FakeConnection conn_;
  std::thread thread_;
};

TEST_F(BinkTest, ErrorAbortsSession) {
  Start();
  conn_.ReplyCommand(BinkpCommands::M_ERR, "Doh!");
  Stop();
  
  while (conn_.has_sent_packets()) {
    std::clog << conn_.GetNextPacket().debug_string() << std::endl;
  }
};
