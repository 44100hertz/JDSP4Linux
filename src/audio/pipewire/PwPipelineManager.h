#ifndef PWPIPELINEMANAGER_H
#define PWPIPELINEMANAGER_H

/*
 *  NOTE: This C++ class is based on code from the EasyEffects/PulseEffects project
 *        and has been adapted to work with JDSP. The original copyright text is attached below:
 *
 *  Copyright © 2017-2022 Wellington Wallace
 *
 *  This file is part of EasyEffects.
 *
 *  EasyEffects is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  EasyEffects is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with EasyEffects.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <glibmm.h>
#include <pipewire/extensions/metadata.h>
#include <pipewire/pipewire.h>
#include <sigc++/sigc++.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>
#include <spa/utils/json.h>
#include <spa/utils/result.h>
#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <map>

struct NodeInfo {
  pw_proxy* proxy = nullptr;

  uint id = SPA_ID_INVALID;

  uint device_id = SPA_ID_INVALID;

  std::string name;

  std::string description;

  std::string media_class;

  std::string app_icon_name;

  std::string media_icon_name;

  std::string device_icon_name;

  std::string media_name;

  std::string format;

  int priority = -1;

  pw_node_state state;

  bool mute = false;

  int n_input_ports = 0;

  int n_output_ports = 0;

  uint rate = 0U;

  int n_volume_channels = 0;

  float latency = 0.0F;

  float volume = 0.0F;
};

struct LinkInfo {
  std::string path;

  uint id = SPA_ID_INVALID;

  uint input_node_id = 0U;

  uint input_port_id = 0U;

  uint output_node_id = 0U;

  uint output_port_id = 0U;

  bool passive = false;  // does not cause the graph to be runnable

  pw_link_state state;
};

struct PortInfo {
  std::string path;

  std::string format_dsp;

  std::string audio_channel;

  std::string name;

  std::string direction;

  bool physical = false;

  bool terminal = false;

  bool monitor = false;

  uint id = SPA_ID_INVALID;

  uint node_id = 0U;

  uint port_id = 0U;
};

struct ModuleInfo {
  uint id;

  std::string name;

  std::string description;

  std::string filename;
};

struct ClientInfo {
  uint id;

  std::string name;

  std::string access;

  std::string api;
};

struct DeviceInfo {
  uint id;

  std::string name;

  std::string description;

  std::string nick;

  std::string media_class;

  std::string api;

  std::string input_route_name;

  std::string output_route_name;

  spa_param_availability input_route_available;

  spa_param_availability output_route_available;
};

class PwPipelineManager {
 public:
  PwPipelineManager();
  PwPipelineManager(const PwPipelineManager&) = delete;
  auto operator=(const PwPipelineManager&) -> PwPipelineManager& = delete;
  PwPipelineManager(const PwPipelineManager&&) = delete;
  auto operator=(const PwPipelineManager&&) -> PwPipelineManager& = delete;
  ~PwPipelineManager();

  const std::string log_tag = "PwPipelineManager: ";

  pw_thread_loop* thread_loop = nullptr;
  pw_core* core = nullptr;
  pw_registry* registry = nullptr;
  pw_metadata* metadata = nullptr;

  spa_hook metadata_listener{};

  std::map<uint, NodeInfo> node_map;

  std::vector<LinkInfo> list_links;

  std::vector<PortInfo> list_ports;

  std::vector<ModuleInfo> list_modules;

  std::vector<ClientInfo> list_clients;

  std::vector<DeviceInfo> list_devices;

  NodeInfo pe_sink_node;

  NodeInfo default_output_device, default_input_device;

  NodeInfo output_device, input_device;

  std::array<std::string, 21> blocklist_node_name = {"EasyEffects",
                                                     "easyeffects",
                                                     "easyeffects_soe",
                                                     "easyeffects_sie",
                                                     "EasyEffectsWebrtcProbe",
                                                     "pavucontrol",
                                                     "PulseAudio Volume Control",
                                                     "libcanberra",
                                                     "gsd-media-keys",
                                                     "GNOME Shell",
                                                     "speech-dispatcher",
                                                     "speech-dispatcher-dummy",
                                                     "Mutter",
                                                     "gameoverlayui",
                                                     "JamesDSP",
                                                     "jamesdsp",
                                                     "GstEffectManager",
                                                     "jdsp-gui",
                                                     "PulseEffectsWebrtcProbe",
                                                     "Screenshot",
                                                     "gst-launch-1.0"};

  std::array<std::string, 2> blocklist_media_role = {"event", "Notification"};

  std::string header_version, library_version, core_name, default_clock_rate, default_min_quantum, default_max_quantum,
      default_quantum;

  auto stream_is_connected(const uint& id, const std::string& media_class) -> bool;

  void connect_stream_output(const uint& id, const std::string& media_class) const;

  void disconnect_stream_output(const uint& id, const std::string& media_class) const;

  static void set_node_volume(pw_proxy* proxy, const int& n_vol_ch, const float& value);

  static void set_node_mute(pw_proxy* proxy, const bool& state);

  /*
    Links the output ports of the node output_node_id to the input ports of the node input_node_id
  */

  auto link_nodes(const uint& output_node_id,
                  const uint& input_node_id,
                  const bool& probe_link = false,
                  const bool& link_passive = true) -> std::vector<pw_proxy*>;

  void destroy_object(const int& id) const;

  /*
    Destroy all the filters links
  */

  void destroy_links(const std::vector<pw_proxy*>& list) const;

  void lock() const;

  void unlock() const;

  void sync_wait_unlock() const;

  static auto json_object_find(const char* obj, const char* key, char* value, const size_t& len) -> int;

  sigc::signal<void(const uint, const std::string, const std::string)> stream_output_added;
  sigc::signal<void(const uint, const std::string, const std::string)> stream_input_added;
  sigc::signal<void(const uint)> stream_output_changed;
  sigc::signal<void(const uint)> stream_input_changed;
  sigc::signal<void(const uint)> stream_output_removed;
  sigc::signal<void(const uint)> stream_input_removed;

  /*
    Do not pass NodeInfo by reference. Sometimes it dies before we use it and a segmentation fault happens.
  */

  sigc::signal<void(NodeInfo)> source_added;
  sigc::signal<void(NodeInfo)> source_changed;
  sigc::signal<void(NodeInfo)> source_removed;
  sigc::signal<void(NodeInfo)> sink_added;
  sigc::signal<void(NodeInfo)> sink_changed;
  sigc::signal<void(NodeInfo)> sink_removed;
  sigc::signal<void(NodeInfo)> new_default_sink;
  sigc::signal<void(NodeInfo)> new_default_source;
  sigc::signal<void(DeviceInfo)> device_input_route_changed;
  sigc::signal<void(DeviceInfo)> device_output_route_changed;

  sigc::signal<void(LinkInfo)> link_changed;

 private:
  bool context_ready = false;

  pw_context* context = nullptr;
  pw_proxy *proxy_stream_output_sink = nullptr, *proxy_stream_input_source = nullptr;

  spa_hook core_listener{}, registry_listener{};
};

#endif
