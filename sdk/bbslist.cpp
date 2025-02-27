/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2022, WWIV Software Services             */
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
#include "sdk/bbslist.h"

#include "core/datafile.h"
#include "core/file.h"
#include "core/graphs.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/filenames.h"
#include "sdk/net/connect.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <iterator>
#include <map>
#include <optional>
#include <sstream>
#include <string>

using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::sdk;

namespace wwiv::sdk {

// [[ VisibleForTesting ]]
bool ParseBbsListNetLine(const std::string& ss, net_system_list_rec* con, int32_t* reg_no) {
  if (ss.empty() || ss[0] != '@') {
    // skip empty lines and those not starting with @.
    return false;
  }
  memset(con, 0, sizeof(net_system_list_rec));
  *reg_no = 0;
  VLOG(2) << ss;

  for (auto iter = ss.begin(); iter != ss.end(); ++iter) {
      switch (*iter) {
      case '@': {
        con->sysnum = to_number<uint16_t>(std::string(++iter, ss.end()));
      } break;
      case '&':
        con->other |= other_net_coord;
        break;
      case '%':
        con->other |= other_group_coord;
        break;
      case '^':
        con->other |= other_area_coord;
        break;
      case '~':
        con->other |= other_subs_coord;
        break;
      case '$':
        con->other |= other_inet;
        break;
      case '\\':
        con->other |= other_fido;
        break;
      case '|':
        con->other |= other_telnet;
        break;
      case '<':
        con->other |= other_no_links;
        break;
      case '>':
        con->other |= other_fts_blt;
        break;
      case '!':
        con->other |= other_direct;
        break;
      case '/':
        con->other |= other_unregistered;
        break;
      case '?':
        con->other |= other_fax;
        break;
      case '_':
        con->other |= other_end_system;
        break;
      case '+':
        con->other |= other_net_server;
        break;
      case '=':
        con->other |= other_unused;
        break;
      // Phone number
      case '*': {
        ++iter;  // skip past *
        std::string phone_number;
        while (iter != ss.end() && !isspace(*iter)) {
          phone_number.push_back(*iter++);
        }
        to_char_array(con->phone, phone_number);
      }
      break;
      case '#': {
        con->speed = to_number<uint16_t>(std::string(++iter, ss.end()));
      } break;
      // Reg Number.
      case '[': {
        ++iter;  // skip past [
        std::string reg_number;
        while (iter != ss.end() && *iter != ']') {
          reg_number.push_back(*iter++);
        }
        *reg_no = to_number<int>(reg_number);
      } break;
      case '\"': {
        ++iter;  // skip past first "
        std::string name;
        while (iter != ss.end() && *iter != '\"') {
          name.push_back(*iter++);
        }
        to_char_array(con->name, name);
      }
      break;
      default:
        break;
      }
    }
    return true;
}

static bool ParseBbsListNetFile(
  std::map<uint16_t, net_system_list_rec>* node_config_map,
  std::map<uint16_t, int32_t>* reg_number_map,
  const std::filesystem::path& network_dir,
  wwiv::graphs::Graph& graph,
  uint16_t net_node_number) {
  TextFile bbs_list_file(FilePath(network_dir, BBSLIST_NET), "rt");
  if (!bbs_list_file.IsOpen()) {
    return false;
  }

  // A line will be of the format @node *phone options [reg] "name"
  std::string line;
  while (bbs_list_file.ReadLine(&line)) {
    StringTrim(&line);
    net_system_list_rec node_config{};
    int32_t reg_number;
    if (ParseBbsListNetLine(line, &node_config, &reg_number)) {
      // Parsed a line correctly.
      auto path = graph.shortest_path_to(node_config.sysnum);
      auto cost = graph.cost_to(node_config.sysnum);
      if (!std::isfinite(cost)) {
        if(VLOG_IS_ON(2)) {
          std::ostringstream ss; 
          VLOG(2) << "high cost " << cost << " to " << node_config.sysnum;
          ss << "Path to " << node_config.sysnum << ": ";
          std::copy(path.begin(), path.end(), std::ostream_iterator<uint16_t>(ss, " "));
          VLOG(2) << ss.str();
          VLOG(2) << graph.DumpCosts();
        }
      }
      if (graph.has_node(node_config.sysnum) && path.front() == net_node_number) {
        path.pop_front();
        // We have a path...
        //std::copy(path.begin(), path.end(), std::ostream_iterator<uint16_t>(std::cout, " "));
        node_config.numhops = static_cast<int16_t>(path.size());
        node_config.xx.cost = cost;
        if (!path.empty()) {
          node_config.forsys = path.front();
        } else {
          node_config.forsys = node_config.sysnum;
        }
      } else {
        VLOG(2) << "no path to " << node_config.sysnum;
        node_config.numhops = 10000;
        node_config.xx.cost = 10000;
        node_config.forsys = std::numeric_limits<uint16_t>::max();
      }
      node_config_map->emplace(node_config.sysnum, node_config);
      reg_number_map->emplace(node_config.sysnum, reg_number);
    }
  }
  return true;
}

// static 
BbsListNet BbsListNet::ParseBbsListNet(uint16_t net_node_number,
                                       const std::filesystem::path& network_dir) {
  BbsListNet b;

  VLOG(3) << "Processing " << network_dir;
  // We now need to add in cost and routing information.
  const Connect connect(network_dir);

  // Build the network graph
  graphs::Graph graph(net_node_number, std::numeric_limits<uint16_t>::max());
  for (const auto& e : connect.node_config()) {
    const auto& c = e.second;
    const auto source = c.sysnum;
    
    auto cost_iter = c.cost.begin();
    for (auto dest_iter = c.connect.begin(); dest_iter != std::end(c.connect); ++dest_iter, ++cost_iter) {
      graph.add_edge(source, *dest_iter, *cost_iter);
    }
  }

  ParseBbsListNetFile(&b.node_config_, &b.reg_number_, network_dir, graph, net_node_number);
  return b;
}

// static 
BbsListNet BbsListNet::ReadBbsDataNet(const std::filesystem::path& network_dir) {
  BbsListNet b;

  if (auto file = DataFile<net_system_list_rec>(FilePath(network_dir, BBSDATA_NET))) {
    std::vector<net_system_list_rec> system_list;
    file.ReadVector(system_list);
    for (const auto& s : system_list) {
      b.node_config_.emplace(s.sysnum, s);
    }
  }
  return b;
}

BbsListNet::BbsListNet() = default;

BbsListNet::BbsListNet(std::initializer_list<net_system_list_rec> l) {
  for (const auto& r : l) {
    node_config_.emplace(r.sysnum, r);
  }
}

BbsListNet::~BbsListNet() = default;

std::optional<net_system_list_rec> BbsListNet::node_config_for(int node) const {
  if (const auto iter = node_config_.find(static_cast<uint16_t>(node)); iter != end(node_config_)) {
    return {iter->second};
  }
  return std::nullopt;
}

static std::string DumpBbsListNet(const net_system_list_rec& n) {
  std::ostringstream ss;
  ss << "sysnum:        " << n.sysnum << std::endl;
  ss << "name:          " << n.name << std::endl;
  ss << "forsys:        " << n.forsys << std::endl;
  ss << "numhops:       " << n.numhops << std::endl;
  ss << "other:         " << n.other << std::endl;
  ss << "phone:         " << n.phone << std::endl;
  ss << "speed:         " << n.speed << std::endl;
  ss << "cost:          " << n.xx.cost << std::endl;
  ss << "rout_fact:     " << n.xx.rout_fact << std::endl;
  return ss.str();
}

std::string BbsListNet::ToString() const {
  std::ostringstream ss;
  for (const auto& kv : node_config_) {
    ss << DumpBbsListNet(kv.second) << std::endl;
  }
  return ss.str();
}

} // namespace wwiv

