#ifndef __HMC_MEMORY_H
#define __HMC_MEMORY_H

#include "HMC.h"
#include "LogicLayer.h"
#include "LogicLayer.cc"
#include "Memory.h"
#include "Packet.h"
#include "Statistics.h"
#include <fstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <array>
#include <climits>
#include <bitset>

using namespace std;

namespace ramulator
{

template<>
class Memory<HMC, Controller> : public MemoryBase
{
protected:
  long max_address;

  long instruction_counter = 0;
  bool profile_this_epoach = true;
  bool get_memory_addresses = false;
  string application_name;
  ofstream memory_addresses;


  long capacity_per_stack;
  ScalarStat dram_capacity;
  ScalarStat num_dram_cycles;
  VectorStat num_read_requests;
  VectorStat num_write_requests;
  ScalarStat ramulator_active_cycles;
  ScalarStat memory_footprint;
  VectorStat incoming_requests_per_channel;
  VectorStat incoming_read_reqs_per_channel;
  ScalarStat physical_page_replacement;
  ScalarStat maximum_internal_bandwidth;
  ScalarStat maximum_link_bandwidth;
  ScalarStat read_bandwidth;
  ScalarStat write_bandwidth;

  ScalarStat read_latency_avg;
  ScalarStat read_latency_ns_avg;
  ScalarStat read_latency_sum;
  ScalarStat queueing_latency_avg;
  ScalarStat queueing_latency_ns_avg;
  ScalarStat queueing_latency_sum;
  ScalarStat request_packet_latency_avg;
  ScalarStat request_packet_latency_ns_avg;
  ScalarStat request_packet_latency_sum;
  ScalarStat response_packet_latency_avg;
  ScalarStat response_packet_latency_ns_avg;
  ScalarStat response_packet_latency_sum;

  // shared by all Controller objects
  ScalarStat read_transaction_bytes;
  ScalarStat write_transaction_bytes;
  ScalarStat row_hits;
  ScalarStat row_misses;
  ScalarStat row_conflicts;
  VectorStat read_row_hits;
  VectorStat read_row_misses;
  VectorStat read_row_conflicts;
  VectorStat write_row_hits;
  VectorStat write_row_misses;
  VectorStat write_row_conflicts;

  ScalarStat req_queue_length_avg;
  ScalarStat req_queue_length_sum;
  ScalarStat read_req_queue_length_avg;
  ScalarStat read_req_queue_length_sum;
  ScalarStat write_req_queue_length_avg;
  ScalarStat write_req_queue_length_sum;

  VectorStat record_read_hits;
  VectorStat record_read_misses;
  VectorStat record_read_conflicts;
  VectorStat record_write_hits;
  VectorStat record_write_misses;
  VectorStat record_write_conflicts;

  long mem_req_count = 0;
  long total_hops = 0;
  bool num_cores;
  int max_block_col_bits;
public:
    long clk = 0;
    bool pim_mode_enabled = false;
    bool network_overhead = false;

    static int calculate_hops_travelled(int src_vault, int dst_vault, int length) {
      assert(src_vault >= 0);
      assert(dst_vault >= 0);
      assert(length >= 0);
      int vault_destination_x = dst_vault/NETWORK_WIDTH;
      int vault_destination_y = dst_vault%NETWORK_WIDTH;

      int vault_origin_x = src_vault/NETWORK_HEIGHT;
      int vault_origin_y = src_vault%NETWORK_HEIGHT;

      int hops = abs(vault_destination_x - vault_origin_x) + abs(vault_destination_y - vault_origin_y);
      hops = hops*length;
      assert(hops <= MAX_HOP);
      return hops;
    }

    static int calculate_hops_travelled(int src_vault, int dst_vault) {
      assert(src_vault >= 0);
      assert(dst_vault >= 0);
      int vault_destination_x = dst_vault/NETWORK_WIDTH;
      int vault_destination_y = dst_vault%NETWORK_WIDTH;

      int vault_origin_x = src_vault/NETWORK_HEIGHT;
      int vault_origin_y = src_vault%NETWORK_HEIGHT;

      int hops = abs(vault_destination_x - vault_origin_x) + abs(vault_destination_y - vault_origin_y);
      assert(hops <= MAX_HOP);
      return hops;
    }

    void set_address_recorder (){
      get_memory_addresses = false;
      string to_open = application_name + ".memory_addresses";
      std::cout << "Recording memory trace at " << to_open << "\n";
      memory_addresses.open(to_open.c_str(), std::ofstream::out);
      memory_addresses << "CLK ADDR W|R Vault BankGroup Bank Row Column \n";
    }

    void set_application_name(string _app){
      application_name = _app;
    }

    enum class Type {
        RoCoBaVa, // XXX The specification doesn't define row/column addressing
        RoBaCoVa,
        RoCoBaBgVa,
        MAX,
    } type = Type::RoCoBaVa;

    std::map<std::string, Type> name_to_type = {
      {"RoCoBaVa", Type::RoCoBaVa},
      {"RoBaCoVa", Type::RoBaCoVa},
      {"RoCoBaBgVa", Type::RoCoBaBgVa}};

    enum class Translation {
      None,
      Random,
      MAX,
    } translation = Translation::None;

    std::map<string, Translation> name_to_translation = {
      {"None", Translation::None},
      {"Random", Translation::Random},
    };

    vector<int> free_physical_pages;
    long free_physical_pages_remaining;
    map<pair<int, long>, long> page_translation;

    vector<list<int>> tags_pools;

    vector<Controller<HMC>*> ctrls;
    vector<LogicLayer<HMC>*> logic_layers;
    HMC * spec;

    enum SubscriptionPrefetcherType {
      None, // Baseline configuration (no prefetching)
      Swap, // Swap with remote vault's same address
      Allocate, // Allocate from local vault's reserved address. To be implemented
      Copy // Copy to local vault's reserved address. To be implemented
    } subscription_prefetcher_type = SubscriptionPrefetcherType::None;

    std::map<string, SubscriptionPrefetcherType> name_to_prefetcher_type = {
      {"None", SubscriptionPrefetcherType::None},
      {"Swap", SubscriptionPrefetcherType::Swap},
      {"Allocate", SubscriptionPrefetcherType::Allocate},
    };

    // A subscription based prefetcher
    class SubscriptionPrefetcherSet {
    private:
      // Subscription task. Denoting where it is subscribing from, and where to, and how many cycles of latency
      struct SubscriptionTask {
        enum Type {
          SubReq, // Request a data to be sent from "to_vault" to "from_vault"
          SubReqAck, // Acknowledge "SubReq". Incorporated into "SubXfer" in real scenario
          SubReqNAck, // Negatively acknowledge "SubReq". Used when "from_vault" has no space so "to_vault" can rollback the subscription
          SubXfer, // Actually transfer the data from "from_vault" to "to_vault"
          SubXferAck, // Acknowledge "SubXferAck"
          UnsubReq, // Request a data currently in "to_vault" be returned to "from_vault"
          UnsubReqAck, // Acknowledge "UnsubReq". Incorporated into "UnsubXfer" in real scenario
          UnsubXfer, // Actually transfer the datat from "from_vault" to "to_vault"
          UnsubXferAck, // Acknowledge "UnsubXfer"
          ResubReq, // Request a data to be sent from "to_vault" to "from_vault", but does not trigger swap
          ResubXfer, // Actually transfer the data from "from_vault" to "to_vault"
          ResubXferAck, // Acknowledge "ResubXfer"
        } type;
        long addr;
        int from_vault;
        int to_vault;
        int hops;
        SubscriptionTask(long addr, int from_vault, int to_vault, int hops, Type type):addr(addr),from_vault(from_vault),to_vault(to_vault),hops(hops),type(type){}
        SubscriptionTask(){}
      };
      class LRUUnit;
      class LFUUnit;
      // The actual subscription table. Translates an address to its subscribed vault
      // Some variables just to save the vaule before initialization
      size_t subscription_table_size = SIZE_MAX;
      size_t subscription_table_ways = subscription_table_size;
      size_t subscription_table_sets = subscription_table_size / subscription_table_ways;
      size_t receiving_buffer_size = 32;
      class SubscriptionTable{
        private:
        bool initialized = false; // Each subscription table can be only initialized once
        // Specs for Subscription table
        size_t subscription_table_size = SIZE_MAX;
        size_t subscription_table_ways = subscription_table_size;
        size_t subscription_table_sets = subscription_table_size / subscription_table_ways;
        size_t receiving_buffer_size = 32;
        // To reserve location in receiving buffer and make sure there is not too many pending subscription/unsubscription at the same time
        size_t receiving = 0;
        LRUUnit* lru_unit = nullptr;
        LFUUnit* lfu_unit = nullptr;
        struct SubscriptionTableEntry {
          int vault;
          enum SubscriptionStatus {
            PendingSubscription,
            Subscribed,
            PendingRemoval,
            PendingResubscription,
            Invalid,
          } status = SubscriptionStatus::PendingSubscription;
          SubscriptionTableEntry(){}
          SubscriptionTableEntry(int vault):vault(vault){}
          SubscriptionTableEntry(int vault, SubscriptionTableEntry::SubscriptionStatus status):vault(vault),status(status){}
        };
        // Actual data structure for those tables
        unordered_map<long, SubscriptionTableEntry> address_translation_table; // Subscribe remote address (1st val) to local address (2nd address)
        vector<size_t> virtualized_table_sets; // Used for limiting the number of ways in each "set" in each subscription table
        public:
        SubscriptionTable(){}
        SubscriptionTable(size_t size, size_t ways, size_t receiving_buffer_size):subscription_table_size(size),subscription_table_ways(ways),receiving_buffer_size(receiving_buffer_size) {initialize();} // Only set from table size
        void set_subscription_table_size(size_t size) {
          subscription_table_size = size;
          // If we have not set the table ways, we make it fully associative to prevent any issues
          if(subscription_table_ways == SIZE_MAX){
            subscription_table_ways = size;
          }
        }
        void set_subscription_table_ways(size_t ways) {subscription_table_ways = ways;}
        size_t get_subscription_table_size()const{return subscription_table_size;}
        size_t get_subscription_table_ways()const{return subscription_table_ways;}
        size_t get_subscription_table_sets()const{return subscription_table_sets;}
        void attach_lru_unit(LRUUnit* ptr) {
          lru_unit = ptr;
        }
        void attach_lfu_unit(LFUUnit* ptr) {
          lfu_unit = ptr;
        }
        void touch_lfu_lru(long addr) {
          if(lru_unit != nullptr) {
            lru_unit -> touch(addr);
          }
          if(lfu_unit != nullptr) {
            lfu_unit -> touch(addr);
          }
        }
        void erase_lfu_lru(long addr) {
          if(lru_unit != nullptr) {
            lru_unit -> erase(addr);
          }
          if(lfu_unit != nullptr) {
            lfu_unit -> erase(addr);
          }
        }
        // We split initialize() function from constructor as it might be called after constructor. It can be only exec'ed once
        void initialize(){
          // We can only initialize once
          assert(!initialized);
          // Initialize the subscription table
          assert(subscription_table_size % subscription_table_ways == 0);
          subscription_table_sets = subscription_table_size / subscription_table_ways;
          cout << "Subscription Table Size: " << (subscription_table_size == SIZE_MAX ? "Unlimited" : to_string(subscription_table_size)) << endl;
          cout << "Subscription Table Ways: " << (subscription_table_ways == SIZE_MAX ? "Unlimited" : to_string(subscription_table_ways)) << endl;
          cout << "Subscription Table Sets: " << subscription_table_sets << endl;
          // One subscription to table per vault
          virtualized_table_sets.assign(subscription_table_sets, 0);
          initialized = true;
        }
        size_t get_set(long addr)const{return addr % subscription_table_sets;}
        bool subscription_table_is_free(long addr, size_t required_space) const {
          return virtualized_table_sets[get_set(addr)] + required_space <= subscription_table_ways;}
        bool receive_buffer_is_free()const{return receiving < receiving_buffer_size;}
        void submit_subscription(int req_vault, long addr){
          virtualized_table_sets[get_set(addr)]++;
          touch_lfu_lru(addr);
          address_translation_table.insert({addr, SubscriptionTableEntry(req_vault, SubscriptionTableEntry::SubscriptionStatus::PendingSubscription)});
        }
        void complete_subscription(long addr) {
          if(has(addr)) {
            address_translation_table.at(addr).status = SubscriptionTableEntry::SubscriptionStatus::Subscribed;
          }
        }
        void rollback_subscription(long addr) {
          complete_unsubscription(addr);
        }
        void submit_unsubscription(long addr) {
          if(has(addr)) {
            address_translation_table.at(addr).status = SubscriptionTableEntry::SubscriptionStatus::PendingRemoval;
          }
        }
        void submit_resubscription(int vault, long addr) {
          if(has(addr)) {
            address_translation_table.at(addr).status = SubscriptionTableEntry::SubscriptionStatus::PendingResubscription;
            address_translation_table.at(addr).vault = vault;
          }
        }
        void modify_subscription(int vault, long addr) {
          if(has(addr)) {
            address_translation_table.at(addr).vault = vault;
          }
        }
        int get_status(long addr)const{
          if(has(addr)) {
            return (int)address_translation_table.at(addr).status;
          }
          return -1;
        }
        void start_receiving() {
          receiving++;
        }
        void stop_receiving() {
          receiving--;
        }
        void complete_unsubscription(long addr) {
          if(has(addr)) {
            erase_lfu_lru(addr);
            // Actually remove the address from the table
            address_translation_table.erase(addr);
            // Decrease the "virtual" set's content count for subscription from the original vault
            if(virtualized_table_sets[get_set(addr)] > 0) {
              virtualized_table_sets[get_set(addr)]--;
            }
          }
        }
        bool has(long addr) const{return address_translation_table.count(addr) > 0;}
        bool has(long addr, int vault)const{
          if(has(addr)){
            return address_translation_table.at(addr).vault == vault;
          }
          return false;
        }
        bool is_subscribed(long addr, int vault)const {
          if(is_subscribed(addr)) {
            return address_translation_table.at(addr).vault == vault;
          }
          return false;
        }
        bool is_subscribed(long addr) const{
          if(has(addr)){
            return address_translation_table.at(addr).status == SubscriptionTableEntry::SubscriptionStatus::Subscribed;
          }
          return false;
        }
        bool is_pending_subscription(long addr) const{
          if(has(addr)){
            return address_translation_table.at(addr).status == SubscriptionTableEntry::SubscriptionStatus::PendingSubscription;
          }
          return false;
        }
        bool is_pending_subscription(long addr, int vault) const{
          if(is_pending_subscription(addr)){
            return address_translation_table.at(addr).vault == vault;
          }
          return false;
        }
        bool is_pending_removal(long addr) const{
          if(has(addr)){
            return address_translation_table.at(addr).status == SubscriptionTableEntry::SubscriptionStatus::PendingRemoval;
          }
          return false;
        }
        bool is_pending_removal(long addr, int vault) const{
          if(is_pending_removal(addr)){
            return address_translation_table.at(addr).vault == vault;
          }
          return false;
        }
        bool is_pending_resubscription(long addr) const{
          if(has(addr)){
            return address_translation_table.at(addr).status == SubscriptionTableEntry::SubscriptionStatus::PendingSubscription;
          }
          return false;
        }
        bool is_pending_resubscription(long addr, int vault) const{
          if(is_pending_resubscription(addr)){
            return address_translation_table.at(addr).vault == vault;
          }
          return false;
        }
        int& operator[](const long& addr){return address_translation_table[addr].vault;}
        size_t count(long addr) const{return address_translation_table.count(addr);}
      };
      vector<SubscriptionTable> subscription_tables;

      // Structures used to evict entry from subscription table when it's full. We can choose from LRU and LFU and "dirty" LFU.
      enum SubscriptionPrefetcherReplacementPolicy {
        LRU,
        LFU,
        DirtyLFU,
      } subscription_table_replacement_policy = SubscriptionPrefetcherReplacementPolicy::LRU; // We default it to LRU
      std::map<string, SubscriptionPrefetcherReplacementPolicy> name_to_prefetcher_rp = {
        {"LRU", SubscriptionPrefetcherReplacementPolicy::LRU},
        {"LFU", SubscriptionPrefetcherReplacementPolicy::LFU},
        {"DirtyLFU", SubscriptionPrefetcherReplacementPolicy::DirtyLFU},
      };
      class LRUUnit {
        private:
        bool initialized = false;
        size_t address_access_history_size = SIZE_MAX; // Currently the size cannot be less than the corresponding table size or it may deadlock (see touch() function below)
        size_t address_access_history_used = 0; // Used for LRU
        size_t corresponding_table_sets = 1;
        vector<list<long>> address_access_history;
        unordered_map<long, typename list<long>::iterator> address_access_history_map;
        public:
        LRUUnit(){}
        LRUUnit(size_t set):corresponding_table_sets(set){initialize();}
        LRUUnit(size_t size, size_t sets):address_access_history_size(size),corresponding_table_sets(sets){initialize();}
        void set_address_access_history_size(size_t size){address_access_history_size = size;}
        void set_corresponding_table_sets(size_t sets){corresponding_table_sets = sets;}
        size_t get_address_access_history_size()const{return address_access_history_size;}
        size_t get_address_access_history_used()const{return address_access_history_used;}
        size_t get_corresponding_table_sets()const{return corresponding_table_sets;}
        size_t get_set(long addr)const{return addr % corresponding_table_sets;}
        void initialize(){
          // We can only initialize once
          assert(!initialized);
          cout << "Address access history size: " << (address_access_history_size == SIZE_MAX ? "Unlimited" : to_string(address_access_history_size));
          cout << " Corresponding table sets: " << corresponding_table_sets << endl;
          address_access_history.assign(corresponding_table_sets, list<long>());
          initialized = true;
        }
        void erase(long addr){
          if(address_access_history_map.count(addr)){
            address_access_history[get_set(addr)].erase(address_access_history_map[addr]);
            address_access_history_map.erase(addr);
            address_access_history_used--;
          }
        }
        void touch(long addr){
          // If there exists the address in access history, we first remove it
          erase(addr);
          // Then if the address access history table is still larger than maximum minus one, we make some space
          // TODO: Make this global
          while(!address_access_history[get_set(addr)].empty() && address_access_history_used >= address_access_history_size) {
            long last = address_access_history[get_set(addr)].back();
            address_access_history[get_set(addr)].pop_back();
            address_access_history_map.erase(last);
            address_access_history_used--;
          }
          // Last, we insert the new address to the front of the history (i.e. most recently accessed)
          address_access_history[get_set(addr)].push_front(addr);
          address_access_history_used++;
          address_access_history_map[addr] = address_access_history[get_set(addr)].begin();
        }
        long find_victim(long addr)const{
          assert(initialized);
          return address_access_history[get_set(addr)].back();
        }
      };
      class LFUUnit {
        private:
        bool initialized = false;
        size_t count_priority_queue_size = SIZE_MAX; // Currently the size cannot be less than the corresponding table size or it may deadlock (see touch() function below)
        size_t count_priority_queue_used = 0;
        size_t corresponding_table_sets = 1;
        struct LFUPriorityQueueItem {
          long addr;
          uint64_t count;
          LFUPriorityQueueItem(){}
          LFUPriorityQueueItem(long addr, uint64_t count):addr(addr),count(count){}
          bool operator< (const LFUPriorityQueueItem& lfupqi)const {
            return this -> count < lfupqi.count;
          }
        };
        vector<multiset<LFUPriorityQueueItem>> count_priority_queue;
        unordered_map<long, typename multiset<LFUPriorityQueueItem>::iterator> count_priority_queue_map;
        public:
        LFUUnit(){}
        LFUUnit(size_t set):corresponding_table_sets(set){initialize();}
        LFUUnit(size_t size, size_t sets):count_priority_queue_size(size),corresponding_table_sets(sets){initialize();}
        void set_count_priority_queue_size(size_t size){count_priority_queue_size = size;}
        void set_corresponding_table_sets(size_t sets){corresponding_table_sets = sets;}
        size_t get_count_priority_queue_size()const{return count_priority_queue_size;}
        size_t get_count_priority_queue_used()const{return count_priority_queue_used;}
        size_t get_corresponding_table_sets()const{return corresponding_table_sets;}
        size_t get_set(long addr)const{return addr % corresponding_table_sets;}
        void initialize(){
          // We can only initialize once
          assert(!initialized);
          cout << "Count Priority Queue size: " << (count_priority_queue_size == SIZE_MAX ? " Unlimited" : to_string(count_priority_queue_size));
          cout << " Corresponding table sets: " << corresponding_table_sets << endl;
          count_priority_queue.assign(corresponding_table_sets, multiset<LFUPriorityQueueItem>()); // Used for LFU
          initialized = true;
        }
        void touch(long addr){
          // First we set the count to 0, in case we are handling new entry
          LFUPriorityQueueItem item(addr, 0);
          // Then, we try to find the existing entry with same address, take it's data to item, increase the count, then remove it from the table pending reinsertion
          if(count_priority_queue_map.count(addr)) {
            auto it = count_priority_queue_map[addr];
            item = *it;
            item.count++;
            count_priority_queue[get_set(addr)].erase(it);
            count_priority_queue_map.erase(addr);
            count_priority_queue_used--;
          }
          // Also, we check if the table has free space, and try make one if it doesn't
          // TODO: Make it global
          while(!count_priority_queue[get_set(addr)].empty() && count_priority_queue_used >= count_priority_queue_size) {
            auto it = count_priority_queue[get_set(addr)].begin();
            long top_addr = it -> addr;
            count_priority_queue[get_set(addr)].erase(it);
            count_priority_queue_map.erase(top_addr);
            count_priority_queue_used--;
          }
          // Finally, we (re)insert the entry into the table
          count_priority_queue_map[addr] = count_priority_queue[get_set(addr)].insert(item);
          count_priority_queue_used++;
        }
        void erase(long addr){
          if(count_priority_queue_map.count(addr)) {
            count_priority_queue[get_set(addr)].erase(count_priority_queue_map[addr]);
            count_priority_queue_map.erase(addr);
            count_priority_queue_used--;
          }
        }
        long find_victim(long addr)const{
          assert(initialized);
          // if(count_priority_queue[get_set(addr)].begin() -> count > 0 || count_priority_queue[get_set(addr)].end() -> count > 0){
            // cout << "The least used address is " << count_priority_queue[get_set(addr)].begin() -> addr << " with count " << count_priority_queue[get_set(addr)].begin() -> count << endl;
            // for(auto& i: count_priority_queue[get_set(addr)]) {
            //   cout << i.addr << " " << i.count << endl;
            // }
            // cout << "The most used address is" << prev(count_priority_queue[get_set(addr)].end()) -> addr << " with count " << prev(count_priority_queue[get_set(addr)].end()) -> count << endl;
          // }
          return count_priority_queue[get_set(addr)].begin() -> addr; // LFU Logic
        }
      };
      vector<LRUUnit> lru_units;
      vector<LFUUnit> lfu_units;

      // Count table. Tracks the # of accesses for each memory location so we can submit for subscription when it reaches threshold
      class CountTable {
        public:
        struct CountTableEntry{
          uint64_t tag;
          uint64_t count;
          CountTableEntry(){}
          CountTableEntry(uint64_t tag, uint64_t count):tag(tag),count(count){}
        };
        private:
        bool initialized = false;
        size_t counter_table_size = 1024;
        int counter_bits = 8;
        int tag_bits = 24;
        int controllers;
        vector<vector<CountTableEntry>> count_tables;
        long evictions = 0;
        long insertions = 0;
        public:
        CountTable(){}
        CountTable(int controllers, size_t size, int counter_bits, int tag_bits):controllers(controllers),counter_table_size(size),counter_bits(counter_bits),tag_bits(tag_bits){initialize();}
        void set_counter_table_size(size_t size){counter_table_size = size;}
        void set_counter_bits(int bits){counter_bits = bits;}
        void set_tag_bits(int bits){tag_bits = bits;}
        void set_controllers(int controllers){this -> controllers = controllers;}
        size_t get_counter_table_size()const{return counter_table_size;}
        int get_counter_bits()const{return counter_bits;}
        int get_tag_bits()const{return tag_bits;}
        int get_controllers()const{return controllers;}
        void initialize(){
          assert(!initialized);
          cout << "Counter table size: " << counter_table_size << " We use " << counter_bits << " bits for counter and " << tag_bits << " bits for tag" << endl;
          count_tables.assign(controllers, vector<CountTableEntry>(counter_table_size, CountTableEntry(0, 0)));
          initialized = true;
        }
        uint64_t calculate_tag(long addr)const{
          const int total_bits = 8*sizeof(uint64_t);
          const int higher_bits = total_bits - tag_bits;
          uint64_t tag = 0;
          while (addr != 0) {
            tag ^= addr;
            addr = addr >> tag_bits;
          }
          tag = (tag << (higher_bits)) >> (higher_bits);
          return tag;
        }
        uint64_t update_counter_table_and_get_count(int req_vault, long addr) {
          uint64_t tag = calculate_tag(addr);
          int index = addr % counter_table_size;
          if(count_tables[req_vault][index].tag != tag){
            // cout << "Inserting " << addr << " address into vault " << req_vault << "'s counter table location " << index << endl;
            count_tables[req_vault][index].tag = tag;
            count_tables[req_vault][index].count = 0;
            evictions++;
          } else {
            count_tables[req_vault][index].count++;
            if(count_tables[req_vault][index].count >= ((uint64_t)1 << counter_bits)) {
              count_tables[req_vault][index].count = ((uint64_t)1 << counter_bits) - 1;
            }
            // cout << "Updating " << addr << " in counter table. It now has count " << count_tables[req_vault][index].count << endl;
          }
          insertions++;
          return count_tables[req_vault][index].count;
        }
        vector<CountTableEntry>& operator[](const size_t& i) {return count_tables[i];}
        void print_stats(){
          cout << "We have accessed " << insertions << " times to the counter table " << " and evicted " << evictions << " from it. The number of accesses without eviction is " << (insertions - evictions) << endl;
        }
      };
      CountTable count_table;

      // Actually dicatates when prefetch happens.
      uint64_t prefetch_hops_threshold = 5;
      uint64_t prefetch_count_threshold = 1;

      // A pointer so we can easily access Memory members
      Memory<HMC, Controller>* mem_ptr = nullptr;

      // Variables used for statistic purposes
      long total_memory_accesses = 0;
      long total_submitted_subscriptions = 0;
      long total_successful_subscriptions = 0;
      long total_unsuccessful_subscriptions = 0;
      long total_unsubscriptions = 0;
      long total_buffer_successful_insertation = 0;
      long total_buffer_unsuccessful_insertation = 0;
      long total_subscription_from_buffer = 0;
      long total_resubscriptions = 0;

      // Tasks being communicated via the network
      list<SubscriptionTask> pending;

      // Buffer to be used when the subscription table is "full". Tasks in this queue is actually at its destination
      size_t subscription_buffer_size = 32; // Anything too large may take long time (days to weeks) to execute for some benchmarks, it shouldn't be too large either as it's a fully-associative queue
      class SubscriptionBuffer {
        private:
        unordered_map<long, typename list<SubscriptionTask>::iterator> map; // To ensure that we don't put two addresses in the same buffer twice
        size_t buffer_size = 32;
        public:
        list<SubscriptionTask> buffer;
        SubscriptionBuffer(){}
        SubscriptionBuffer(size_t buffer_size):buffer_size(buffer_size){}
        bool is_free()const{return buffer.size() < buffer_size;}
        bool is_not_empty()const{return buffer.size() > 0;}
        bool has(long addr)const{return map.count(addr) > 0;}
        void erase(long addr){
          buffer.erase(map[addr]);
          map.erase(addr);
        }
        void push_back(const SubscriptionTask& task) {
          if(buffer.size() < buffer_size && !has(task.addr)){
            buffer.push_back(task);
            map[task.addr] = prev(end());
          }
        }
        typename list<SubscriptionTask>::iterator begin(){return buffer.begin();}
        typename list<SubscriptionTask>::iterator end(){return buffer.end();}
        
      };
      vector<SubscriptionBuffer> subscription_buffers;
      int controllers; // Record how many vaults we have
      // Control if we swap the subscribed address with its "mirror" address. Subscription
      // SubscriptionPrefetcherType::Swap set this to true, SubscriptionPrefetcherType::Allocate set this to false
      bool swap = false;
      int tailing_zero = 1;
      bool debug = false; // Used to controll debug dump
    public:
      explicit SubscriptionPrefetcherSet(int controllers, Memory<HMC, Controller>* mem_ptr):controllers(controllers),mem_ptr(mem_ptr) {
        tailing_zero = mem_ptr -> tx_bits + 1;
        count_table.set_controllers(controllers);
      }
      void print_debug_info(const string& info) {
        if(debug) {
          cout << "[Subscription Debug] " << info << endl;
        }
      }
      void set_debug_flag(bool flag) {
        debug = flag;
        print_debug_info("Debug Mode On.");
      }
      vector<int> address_to_address_vector(long addr){
        addr <<= tailing_zero;
        return mem_ptr -> address_to_address_vector(addr);
      }
      long address_vector_to_address(const vector<int> addr_vec) {
        long addr = mem_ptr -> address_vector_to_address(addr_vec);
        addr >>= tailing_zero;
        return addr;
      }
      int find_original_vault_of_address(long addr){
        vector<int> addr_vec = address_to_address_vector(addr);
        // cout << "The original vault of address " << addr << " is " << addr_vec[int(HMC::Level::Vault)] << endl;
        return addr_vec[int(HMC::Level::Vault)];
      }
      long find_victim_for_unsubscription(int vault, long addr) const {
        long victim_addr = 0;
        if(subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::LRU) {
          victim_addr = lru_units[vault].find_victim(addr); // LRU Logic
        } else if(subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::LFU) {
          // cout << "We found victim " << lfu_unit.find_victim(addr) << endl;
          victim_addr = lfu_units[vault].find_victim(addr); // LFU Logic
        } else if(subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::DirtyLFU){
          assert(false);
        } else {
          // If our replacement policy is unknown, we fail deliberately
          assert(false);
        }
        return victim_addr;
      }
      void set_prefetch_hops_threshold(int threshold) {
        prefetch_hops_threshold = threshold;
        cout << "Prefetcher hops threshold: " << prefetch_hops_threshold << endl;
      }
      void set_prefetch_count_threshold(int threshold) {
        prefetch_count_threshold = threshold;
        cout << "Prefetcher count threshold: " << prefetch_count_threshold << endl;
      }
      void set_subscription_table_size(size_t size) {
        if(subscription_table_ways == SIZE_MAX){
          subscription_table_ways = size;
        }
        subscription_table_size = size;
      }
      void set_subscription_table_ways(size_t ways) {subscription_table_ways = ways;}
      void set_subscription_buffer_size(size_t size) {subscription_buffer_size = size;}
      void set_subscription_recv_buffer_size(size_t size) {receiving_buffer_size = size;}
      void set_subscription_table_replacement_policy(const string& policy) {
        subscription_table_replacement_policy = name_to_prefetcher_rp[policy];
        cout << "Subscription table replacement policy is: " << policy << endl;
      }
      void set_counter_table_size(size_t size) {count_table.set_counter_table_size(size);}
      void set_counter_bits(int bits) {count_table.set_counter_bits(bits);}
      void set_tag_bits(int bits) {count_table.set_tag_bits(bits);}
      void set_swap_switch(bool val) {swap = val;}
      void initialize_sets(){
        subscription_tables.assign(controllers, SubscriptionTable(subscription_table_size, subscription_table_ways, SIZE_MAX));
        subscription_table_sets = subscription_table_size / subscription_table_ways;
        count_table.initialize();
        if(subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::LRU) {
          lru_units.assign(controllers, LRUUnit(subscription_table_sets));
          for(int c = 0; c < controllers; c++) {
            subscription_tables[c].attach_lru_unit(&lru_units[c]);
          }
        } else if(subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::LFU) {
          lfu_units.assign(controllers, LFUUnit(subscription_table_sets));
          for(int c = 0; c < controllers; c++) {
            subscription_tables[c].attach_lfu_unit(&lfu_units[c]);
          }
        } else if(subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::DirtyLFU) {
          cout << "DirtyLFU is no longer supported due to it causing starving in the buffer" << endl;
          assert(false);
        } else {
          cout << "Unknown replacement policy!" << endl;
          assert(false); // We fail early if the policy is not known.
        }
        cout << "Subscription buffer size: " << subscription_buffer_size << endl;
        subscription_buffers.assign(controllers, SubscriptionBuffer(subscription_buffer_size));
      }
      bool check_prefetch(uint64_t hops, uint64_t count) const {
        // TODO: Use machine learning to optimize prefetching
        return hops >= prefetch_hops_threshold && count >= prefetch_count_threshold;
      }
      long find_mirror_address(int vault, long addr) {
        vector<int> victim_vec = address_to_address_vector(addr);
        victim_vec[int(HMC::Level::Vault)] = vault;
        long victim_addr = address_vector_to_address(victim_vec);
        return victim_addr;
      }
      // Start the entire process by allocating subscription table locally, and push a subscription request into the network or buffer
      void subscribe_address(int req_vault, long addr) {
        // Starting by reserving space in the subscription table. For swap we need 2 entries (one for the actual subscription, one for the swapped out address)
        int required_space = swap ? 2 : 1;
        // Calculate vaules needed for next steps
        int original_vault = find_original_vault_of_address(addr);
        int hops = calculate_hops_travelled(req_vault, original_vault);
        // Generate a "Subscription Request" task
        SubscriptionTask task = SubscriptionTask(addr, req_vault, original_vault, hops, SubscriptionTask::Type::SubReq);
        // cout << "SubReq task generated from " << task.from_vault << " to " << task.to_vault << " addr " << task.addr << endl;
        // If the original vault of the address is the current vault (i.e. we have an address subscribed elsewhere and we want it back), we need to process unsubscription
        if(original_vault == req_vault) {
          // cout << "addr " << addr << " is currently in the same to vault as subscribe vault" << endl;
          unsubscribe_address(req_vault, addr);
        // If we have space to insert it into the subscription table and to receive the data, we push it into the network
        } else if(subscription_tables[req_vault].receive_buffer_is_free() && subscription_tables[req_vault].subscription_table_is_free(addr, required_space)){
          // cout << "we push address " << addr << " into the network" << endl;
          push_subscribe_request_into_network(task);
        // Otherwise, we push it into the subscription buffer of the requesting vault, pending unsubscription that frees up the table
        } else {
          // cout << "either the receive buffer is full or subscription table is full. We wait..." << endl;  
          // If this failure of pushing into network is due to lack of subscription table space, we try to make some space
          if(!subscription_tables[req_vault].subscription_table_is_free(addr, required_space)) {
            // We find a victim address to free up subscription table
            long victim_addr = find_victim_for_unsubscription(req_vault, addr);
            // We then submit the address for unsubscribe
            unsubscribe_address(task.from_vault, victim_addr);
          }
          // If we have space, we insert it into the buffer
          if(subscription_buffers[req_vault].is_free()){
            total_buffer_successful_insertation++;
            print_debug_info("Pushing task "+to_string(task.addr)+" from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" into the buffer at sender");
            subscription_buffers[req_vault].push_back(task);
          // Otherwise, there is nothing we can do
          } else {
            total_buffer_unsuccessful_insertation++;
          }
        }
      }
      // Finish pushing the subscription request into the network
      void push_subscribe_request_into_network(const SubscriptionTask& task) {
        assert(task.type == SubscriptionTask::Type::SubReq);
        // We then insert the subscription request into the table, and allocate space in receiving buffer for receiving
        print_debug_info("Submitting address "+to_string(task.addr)+" into "+to_string(task.from_vault)+" to "+to_string(task.to_vault));
        subscription_tables[task.from_vault].submit_subscription(task.from_vault, task.addr);
        subscription_tables[task.from_vault].start_receiving();
        print_debug_info("After submission, entry "+to_string(task.addr)+" exists in from vault? "+to_string(subscription_tables[task.from_vault].has(task.addr))+" status? "+to_string(subscription_tables[task.from_vault].get_status(task.addr))+" To which vault? "+to_string(subscription_tables[task.from_vault][task.addr]));
        print_debug_info("After submission, entry "+to_string(task.addr)+" exists in to vault? "+to_string(subscription_tables[task.to_vault].has(task.addr))+" status? "+to_string(subscription_tables[task.to_vault].get_status(task.addr)));
        // Actually put the request into the network
        print_debug_info("We are pushing subscription task from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" with addr "+to_string(task.addr));
        pending.push_back(task);
        total_submitted_subscriptions++;
        // If we want to swap, we reserve the space for swapped out address as well
        if(swap) {
          long mirror_addr = find_mirror_address(task.from_vault, task.addr);
          subscription_tables[task.from_vault].submit_subscription(task.to_vault, mirror_addr);
        }
      }
      // Process received subscription request. Start data transfer or push it into the buffer or return failure
      void receive_subscribe_request(const SubscriptionTask& task) {
        assert(task.type == SubscriptionTask::Type::SubReq);
        // Starting by reserving space in the subscription table. For swap we need 2 entries (one for the actual subscription, one for the swapped out address)
        int required_space = swap ? 2 : 1;
        // If we have space in subscription table to put it in, and receiving buffer to receive the swapped out address in the case of swap, we proceed with subscription
        if(subscription_tables[task.to_vault].subscription_table_is_free(task.addr, required_space) && (!swap || subscription_tables[task.to_vault].receive_buffer_is_free())) {
          process_subscribe_request(task);
        // If not, but we have some space in the buffer, we insert it into the buffer
        } else if(subscription_buffers[task.to_vault].is_free()) {
          total_buffer_successful_insertation++;
          print_debug_info("Pushing task "+to_string(task.addr)+" from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" into the buffer at receiver");
          subscription_buffers[task.to_vault].push_back(task);
          long victim_addr = find_victim_for_unsubscription(task.to_vault, task.addr);
          unsubscribe_address(task.to_vault, victim_addr);
        // Otherwise, there is nothing we can do
        } else {
          total_buffer_unsuccessful_insertation++;
          print_debug_info("We are pushing SubReqNack task from "+to_string(task.to_vault)+" to "+to_string(task.from_vault)+" with addr "+to_string(task.addr));
          pending.push_back(SubscriptionTask(task.addr, task.to_vault, task.from_vault, task.hops, SubscriptionTask::Type::SubReqNAck));
          long victim_addr = find_victim_for_unsubscription(task.to_vault, task.addr);
          unsubscribe_address(task.to_vault, victim_addr);
        }
      }
      // Finish processing subscription request
      void process_subscribe_request(const SubscriptionTask& task) {
        assert(task.type == SubscriptionTask::Type::SubReq);
        // We calculate how many hops it required to transfer the data
        int hops = calculate_hops_travelled(task.to_vault, task.from_vault);
        // If this address is not currently subscribed anywhere, we insert the subscription request into the table, and allocate space in receiving buffer for receiving swapped out address
        if(!subscription_tables[task.to_vault].has(task.addr)){
          print_debug_info("Submitting address "+to_string(task.addr)+" from "+to_string(task.from_vault)+" into "+to_string(task.to_vault));
          subscription_tables[task.to_vault].submit_subscription(task.from_vault, task.addr);
          print_debug_info("After submission, entry "+to_string(task.addr)+" exists in from vault? "+to_string(subscription_tables[task.from_vault].has(task.addr))+" status? "+to_string(subscription_tables[task.from_vault].get_status(task.addr)));
          print_debug_info("After submission, entry "+to_string(task.addr)+" exists in to vault? "+to_string(subscription_tables[task.to_vault].has(task.addr))+" status? "+to_string(subscription_tables[task.to_vault].get_status(task.addr))+" To which vault? "+to_string(subscription_tables[task.to_vault][task.addr]));
          // We send the acknowledgement and data through the network
          print_debug_info("We are pushing SubReqAck and SubXfer task from "+to_string(task.to_vault)+" to "+to_string(task.from_vault)+" with addr "+to_string(task.addr));
          pending.push_back(SubscriptionTask(task.addr, task.to_vault, task.from_vault, hops, SubscriptionTask::Type::SubReqAck));
          pending.push_back(SubscriptionTask(task.addr, task.to_vault, task.from_vault, hops*WRITE_LENGTH, SubscriptionTask::Type::SubXfer));
          // If swap is on, we compute the "mirror" address and swap it
          if(swap) {
            long mirror_addr = find_mirror_address(task.from_vault, task.addr);
            subscription_tables[task.to_vault].submit_subscription(task.to_vault, mirror_addr);
            subscription_tables[task.to_vault].start_receiving();
          }
        // If it is already subscribed, and not in the process of removal or subscription, we submit re-subscription request to ask the current subscribed vault to send it
        } else if(subscription_tables[task.to_vault].is_subscribed(task.addr)){
          int value_vault = subscription_tables[task.to_vault][task.addr];
          int to_vaule_hops = calculate_hops_travelled(task.from_vault, value_vault);
          // We send acknowledgement to the requester, and ask the vault vault to send the vaule back to the requester
          print_debug_info("We are pushing ResubReq task from "+to_string(task.from_vault)+" to "+to_string(value_vault)+" with addr "+to_string(task.addr));
          pending.push_back(SubscriptionTask(task.addr, task.from_vault, value_vault, to_vaule_hops, SubscriptionTask::Type::ResubReq));
          if(swap) {
            // In the case of swap, we also return the swapped out data of the original subscriber as it is no longer needed
            long mirror_addr = find_mirror_address(value_vault, task.addr);
            pending.push_back(SubscriptionTask(task.addr, task.to_vault, value_vault, hops*WRITE_LENGTH, SubscriptionTask::Type::UnsubXfer));
          }
        // If it is in the process of removal or subscription and we are not in the process of subscribing from or removing to the requester vault, we cannot really subscribe it
        } else if(subscription_tables[task.to_vault][task.addr] != task.from_vault) {
          print_debug_info("We are pushing SubReqNAck task from "+to_string(task.to_vault)+" to "+to_string(task.from_vault)+" with addr "+to_string(task.addr));
          pending.push_back(SubscriptionTask(task.addr, task.to_vault, task.from_vault, hops, SubscriptionTask::Type::SubReqNAck));
        }
      }
      // Used only when swap is set as true. For find swapout data and actually swap it out
      void start_mirror_subscription_transfer(const SubscriptionTask& task) {
        assert(task.type == SubscriptionTask::Type::SubReqAck);
        if(swap) {
          long mirror_addr = find_mirror_address(task.to_vault, task.addr);
          int hops = calculate_hops_travelled(task.to_vault, task.from_vault, WRITE_LENGTH);
          pending.push_back(SubscriptionTask(mirror_addr, task.to_vault, task.from_vault, hops, SubscriptionTask::Type::SubXfer));
        }
      }
      // Upon receiving negative acknowledgement, rollback the pending subscription
      void subscription_rollback(const SubscriptionTask& task) {
        assert(task.type == SubscriptionTask::Type::SubReqNAck);
        print_debug_info("Rollback address "+to_string(task.addr));
        subscription_tables[task.to_vault].rollback_subscription(task.addr);
        total_unsuccessful_subscriptions++;
      }
      // Upon receiving transferred data, make the subscription final and provide acknowledgement (for client side)
      void receive_subscription_transfer(const SubscriptionTask& task) {
        assert(task.type == SubscriptionTask::Type::SubXfer || task.type == SubscriptionTask::Type::ResubXfer);
        // Mark the local entry of the subscription table as completed and free up the receiving buffer
        print_debug_info("Complete subscription "+to_string(task.addr)+" from "+to_string(task.from_vault)+" into "+to_string(task.to_vault));
        subscription_tables[task.to_vault].complete_subscription(task.addr);
        print_debug_info("After completion, entry "+to_string(task.addr)+" exists in from vault? "+to_string(subscription_tables[task.from_vault].has(task.addr))+" status? "+to_string(subscription_tables[task.from_vault].get_status(task.addr)));
        print_debug_info("After completion, entry "+to_string(task.addr)+" exists in to vault? "+to_string(subscription_tables[task.to_vault].has(task.addr))+" status? "+to_string(subscription_tables[task.to_vault].get_status(task.addr)));
        subscription_tables[task.to_vault].stop_receiving();
        // Calculate the hops to original and from vault (they are not the same in the case of resubscription)
        int original_vault = find_original_vault_of_address(task.addr);
        int original_vault_hops = calculate_hops_travelled(original_vault, task.to_vault);
        int hops = calculate_hops_travelled(task.to_vault, task.from_vault);
        // Send the acknowledgement to the original vault so it can update its entry accordingly
        print_debug_info("We are pushing SubXferAck task from "+to_string(task.to_vault)+" to "+to_string(original_vault)+" with addr "+to_string(task.addr));
        pending.push_back(SubscriptionTask(task.addr, task.to_vault, original_vault, original_vault_hops, SubscriptionTask::Type::SubXferAck));
        // If it is resubscription, we also need to notify the from vault (where the data is actually from) to ensure that it can remove that entry from its table
        if(task.type == SubscriptionTask::Type::ResubXfer) {
          pending.push_back(SubscriptionTask(task.addr, task.to_vault, task.from_vault, hops, SubscriptionTask::Type::ResubXferAck));
        }
        total_successful_subscriptions++;
      }
      // Upon receiving acknowledgement, make the subscription final/process resubscription based on the reply (for value side)
      void finish_subscription_transfer(const SubscriptionTask& task) {
        assert(task.type == SubscriptionTask::Type::SubXferAck);
        // If we already have this address subscribed (which is resubscription), we modify existing subscription entry to re-direct the 
        if(subscription_tables[task.to_vault].is_subscribed(task.addr)) {
          subscription_tables[task.to_vault].modify_subscription(task.from_vault, task.addr);
        // If not, we change it to completed
        } else if(subscription_tables[task.to_vault].is_pending_subscription(task.addr)){
          print_debug_info("Complete subscription "+to_string(task.addr)+" from "+to_string(task.from_vault)+" into "+to_string(task.to_vault));
          subscription_tables[task.to_vault].complete_subscription(task.addr);
          print_debug_info("After completion, entry "+to_string(task.addr)+" exists in from vault? "+to_string(subscription_tables[task.from_vault].has(task.addr))+" status? "+to_string(subscription_tables[task.from_vault].get_status(task.addr)));
          print_debug_info("After completion, entry "+to_string(task.addr)+" exists in to vault? "+to_string(subscription_tables[task.to_vault].has(task.addr))+" status? "+to_string(subscription_tables[task.to_vault].get_status(task.addr)));
        }
      }
      // Start the unsubscription process by determining if the unsubscription is made by the holder of the address, and act accordingly
      void unsubscribe_address(int from_vault, long addr) {
        if(!subscription_tables[from_vault].has(addr)) {
          print_debug_info("We have addr "+to_string(addr)+" for subscription from vault "+to_string(from_vault)+" but we do not have it");
        }
        assert(subscription_tables[from_vault].has(addr));
        // Find out where the addressis currently at and where its original vault is
        int current_vault = subscription_tables[from_vault][addr];
        int original_vault = find_original_vault_of_address(addr);
        // The from_vault might be either the original vault or current vault, so we need to calculate both the hops from from vault to current vault
        // And from from vault to original vault. One of those hop counts will be 0
        int hops = calculate_hops_travelled(from_vault, current_vault);
        int reverse_hops = calculate_hops_travelled(from_vault, original_vault);
        SubscriptionTask task = SubscriptionTask(addr, from_vault, current_vault, hops, SubscriptionTask::Type::UnsubReq);
        // If we are calling from the original vault, we need to send UnsubReq through the network to the current vault so it can send data back
        if(from_vault == original_vault) {
          print_debug_info("We are pushing UnsubReq task from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" with addr "+to_string(task.addr));
          pending.push_back(task);
          // But at the same time, we also send the swapped out data back to its original vault
          if(swap) {
            long mirror_addr = find_mirror_address(addr, current_vault);
            process_unsubscribe_request(SubscriptionTask(mirror_addr, from_vault, original_vault, reverse_hops, SubscriptionTask::Type::UnsubReq));
          }
        // If we're calling from the current vault, we can process the unsubscription immediately by sending the data back to its original vault
        } else if(from_vault == current_vault) {
          process_unsubscribe_request(task);
          // And we also request swapped vault back
          if(swap) {
            long mirror_addr = find_mirror_address(addr, original_vault);
            pending.push_back(SubscriptionTask(mirror_addr, from_vault, original_vault, reverse_hops, SubscriptionTask::Type::UnsubReq));
          }
        } else {
          print_debug_info("we are unsubscribing "+to_string(task.addr)+" with original vault "+to_string(original_vault)+" and vault vault "+to_string(current_vault)+" this is requested by "+to_string(from_vault));
          assert(false); // The vault requesting unsubscription should either be the original vault or current vault
        }
      }
      // Actually process the unsubscription process (either by receiving UnsubReq or by starting unsubscription from current vault)
      void process_unsubscribe_request(const SubscriptionTask& task) {
        assert(task.type == SubscriptionTask::Type::UnsubReq);
        int original_vault = find_original_vault_of_address(task.addr);
        // If the address is currently subscribed or pending subscription, we just start unsubscription
        if(subscription_tables[task.to_vault].is_subscribed(task.addr) || subscription_tables[task.to_vault].is_pending_subscription(task.addr)) {
          // We first mark the subscription table as "pending removal"
          subscription_tables[task.to_vault].submit_unsubscription(task.addr);
          // We again find the original vault of the address to send the data back
          // The to_vault will always be the "current vault" holding the address
          int hops = calculate_hops_travelled(task.to_vault, original_vault);
          // It seems we do not need to send acknowledgement in the case of unsubscription request
          // if(task.to_vault != task.from_vault) {
          //   pending.push_back(SubscriptionTask(task.addr, task.to_vault, task.from_vault, hops, SubscriptionTask::Type::UnsubReqAck));
          // }
          // Last, we actually send the data back to the original vault
          print_debug_info("We are pushing UnsubXfer task from "+to_string(task.to_vault)+" to "+to_string(original_vault)+" with addr "+to_string(task.addr));
          pending.push_back(SubscriptionTask(task.addr, task.to_vault, original_vault, hops*WRITE_LENGTH, SubscriptionTask::Type::UnsubXfer));
        // If it is the address is currently being resubscribed (i.e. moved from this vault to another vault)
        // We do nothing in the case of local unsubscription (requested by current vault) and forward the subscription request to the new location if it is remote unsubscription (requested by the original vault)
        } else if(subscription_tables[task.to_vault].is_pending_resubscription(task.addr) && task.from_vault == original_vault) {
          int future_subscribed_vault = subscription_tables[task.to_vault][task.addr];
          int hops = calculate_hops_travelled(task.to_vault, future_subscribed_vault);
          print_debug_info("We are pushing UnsubXfer task from "+to_string(original_vault)+" to "+to_string(future_subscribed_vault)+" with addr "+to_string(task.addr));
          pending.push_back(SubscriptionTask(task.addr, original_vault, future_subscribed_vault, hops, SubscriptionTask::Type::UnsubReq));
        } else {
          print_debug_info("We are unable to subscribe address "+to_string(task.addr)+" from "+to_string(task.from_vault)+" to "+to_string(task.to_vault));
          print_debug_info("Do we have this address "+to_string(subscription_tables[task.to_vault].has(task.addr)));
          print_debug_info("Its status: "+to_string(subscription_tables[task.to_vault].get_status(task.addr)));
        }
      }
      // Process unsubscription data transfer
      void process_unsubscribe_transfer(const SubscriptionTask& task) {
        assert(task.type == SubscriptionTask::Type::UnsubXfer);
        // Then we complete unsubscription
        int hops = calculate_hops_travelled(task.to_vault, task.from_vault);
        print_debug_info("Complete unsubscription of address "+to_string(task.addr));
        subscription_tables[task.to_vault].complete_unsubscription(task.addr);
        // And send an acknowledgement back to the "current vault" so it knows it can safely erase this entry from the subscription table
        pending.push_back(SubscriptionTask(task.addr, task.to_vault, task.from_vault, hops, SubscriptionTask::Type::UnsubXferAck));
      }
      // Completes unsubscription data transfer
      void complete_unsubscribe_transfer(const SubscriptionTask& task) {
        assert(task.type == SubscriptionTask::Type::UnsubXferAck);
        // Then update subscription table
        print_debug_info("Complete unsubscription of address "+to_string(task.addr));
        subscription_tables[task.to_vault].complete_unsubscription(task.addr);
        total_unsubscriptions++;
      }
      // Process resubscription request
      void process_resubscription_request(const SubscriptionTask& task) {
        assert(task.type == SubscriptionTask::Type::ResubReq);
        int hops = calculate_hops_travelled(task.to_vault, task.from_vault);
        // If this entry is anything other than subscribed, we cannot resubscribe it, so we negatively acknowledgement this resubscription request to ask the sender to try later
        if(!subscription_tables[task.to_vault].is_subscribed(task.addr)) {
          pending.push_back(SubscriptionTask(task.addr, task.to_vault, task.from_vault, hops, SubscriptionTask::Type::SubReqNAck));
        // Otherwise, we can transfer the data to the current requester
        } else {
          subscription_tables[task.to_vault].submit_resubscription(task.from_vault, task.addr);
          pending.push_back(SubscriptionTask(task.addr, task.to_vault, task.from_vault, hops, SubscriptionTask::Type::SubReqAck));
          pending.push_back(SubscriptionTask(task.addr, task.to_vault, task.from_vault, hops*WRITE_LENGTH, SubscriptionTask::Type::ResubXfer));
        }
      }
      // Finish resubscription request
      void finish_resubscription_transfer(const SubscriptionTask& task) {
        assert(task.type == SubscriptionTask::Type::ResubXferAck);
        // Upon receiving Resubscription transfer acknowledgement, we can savely remove this entry from our subscription table as it is no longer needed
        print_debug_info("Complete unsubscription of address "+to_string(task.addr));
        subscription_tables[task.to_vault].complete_unsubscription(task.addr);
        total_resubscriptions++;
      }
      // Go into the buffer and check if we can process the subscription request, and if so, completes such request
      bool process_task_from_buffer(const SubscriptionTask& task) {
        // We first calculate the required space if we are inserting it
        int required_space = swap ? 2 : 1;
        // If hops is not 0, it means this task is from the sender's end
        if(task.hops != 0) {
          // If this task is already completed and the entry is there, we do nothing and ask the call stack to remove this task
          if(subscription_tables[task.from_vault].is_subscribed(task.addr, task.from_vault) || subscription_tables[task.from_vault].is_pending_subscription(task.addr) || subscription_tables[task.from_vault].is_pending_removal(task.addr)) {
            return true;
          // Otherwise, we see if we can insert this task into the subscription table
          } else if(subscription_tables[task.from_vault].receive_buffer_is_free() && subscription_tables[task.from_vault].subscription_table_is_free(task.addr, required_space)) {
            print_debug_info("Processing task addr "+to_string(task.addr)+" from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" with hop "+to_string(task.hops));
            // Then we insert it
            push_subscribe_request_into_network(task);
            // And ask the it to be removed
            return true;
          }
        // Otherwise, it is in the receiver's end
        } else if(task.hops == 0) {
          // Same, check if it is completed
          if(subscription_tables[task.to_vault].is_subscribed(task.addr, task.from_vault) || subscription_tables[task.to_vault].is_pending_subscription(task.addr) || subscription_tables[task.to_vault].is_pending_removal(task.addr)) {
            return true;
          }
          // Check for insertion condition and insert
          else if(subscription_tables[task.to_vault].subscription_table_is_free(task.addr, required_space) && (!swap || subscription_tables[task.to_vault].receive_buffer_is_free())) {
            print_debug_info("Processing task addr "+to_string(task.addr)+" from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" with hop "+to_string(task.hops));
            process_subscribe_request(task);
            return true;
          }
        }
        return false;
      }
      // Process tasks currently in the network depends on the request type
      void process_task_from_network(const SubscriptionTask& task) {
        switch(task.type){
          case SubscriptionTask::Type::SubReq:
            print_debug_info("Recv SubReq from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" addr "+to_string(task.addr));
            receive_subscribe_request(task);
            break;
          case SubscriptionTask::Type::SubReqAck:
            print_debug_info("Recv SubReqAck from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" addr "+to_string(task.addr));
            start_mirror_subscription_transfer(task);
            break;
          case SubscriptionTask::Type::SubReqNAck:
            print_debug_info("Recv SubReqNAck from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" addr "+to_string(task.addr));
            subscription_rollback(task);
            break;
          case SubscriptionTask::Type::SubXfer:
            print_debug_info("Recv SubXfer from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" addr "+to_string(task.addr));
            receive_subscription_transfer(task);
            break;
          case SubscriptionTask::Type::SubXferAck:
            print_debug_info("Recv SubXferAck from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" addr "+to_string(task.addr));
            finish_subscription_transfer(task);
            break;
          case SubscriptionTask::Type::UnsubReq:
            print_debug_info("Recv UnsubReq from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" addr "+to_string(task.addr));
            process_unsubscribe_request(task);
            break;
          case SubscriptionTask::Type::UnsubReqAck:
            print_debug_info("Recv UnsubReqAck from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" addr "+to_string(task.addr));
          // It seems we do not need to do anything in the case of unsubscription request acknowledgement
            break;
          case SubscriptionTask::Type::UnsubXfer:
            print_debug_info("Recv UnsubXfer from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" addr "+to_string(task.addr));
            process_unsubscribe_transfer(task);
            break;
          case SubscriptionTask::Type::UnsubXferAck:
            print_debug_info("Recv UnsubXferAck from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" addr "+to_string(task.addr));
            complete_unsubscribe_transfer(task);
            break;
          case SubscriptionTask::Type::ResubReq:
            print_debug_info("Recv ResubReq from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" addr "+to_string(task.addr));
            process_resubscription_request(task);
            break;
          case SubscriptionTask::Type::ResubXfer:
            print_debug_info("Recv ResubXfer from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" addr "+to_string(task.addr));
            receive_subscription_transfer(task);
            break;
          case SubscriptionTask::Type::ResubXferAck:
            print_debug_info("Recv ResubXferAck from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" addr "+to_string(task.addr));
            finish_resubscription_transfer(task);
            break;
          default:
            assert(false);
        }
      }

      void tick() {
        // First, we check if there is any subscription buffer in pending (i.e. arrived but cannot be subscribed due to subscription table space constraints)
        for(int controller = 0; controller < controllers; controller++){
          if(subscription_buffers[controller].is_not_empty()) {
            SubscriptionTask task = subscription_buffers[controller].buffer.front();
            if(process_task_from_buffer(task)) {
              subscription_buffers[controller].buffer.pop_front();
              total_subscription_from_buffer++;
            }
            // SubscriptionBuffer new_subscription_buffer;
            // for(auto& i:subscription_buffers[controller]) {
            //   if(!process_task_from_buffer(i)) {
            //     new_subscription_buffer.push_back(i);
            //   } else {
            //     total_subscription_from_buffer++;
            //   }
            // }
            // subscription_buffers[controller] = new_subscription_buffer;
          }
        }
        // Then, we process the transfer of subscription requests in the network
        list<SubscriptionTask> new_pending;
        for(auto& i:pending) {
          if(i.hops == 0) {
            process_task_from_network(i);
          } else {
            i.hops -= 1;
            new_pending.push_back(i);
          }
        }
        pending = new_pending;
      }
      void pre_process_addr(long& addr) {
        mem_ptr -> clear_lower_bits(addr, mem_ptr -> tx_bits + tailing_zero);
      }
      // A rewritten function that updates LRU entries, translates old address to the correct vault, and then update counter table and check for prefetch
      void access_address(Request& req) {
        // Preprocess the address as we need to drop some lowest bits
        long addr = req.addr;
        pre_process_addr(addr);
        // We take the requester's vault # for easier processing
        int req_vault_id = req.coreid;
        int original_vault_id = req.addr_vec[int(HMC::Level::Vault)];
        int val_vault_id = original_vault_id;
        // If we have the address in the subscription table, we have it in LRU or LFU unit, and we send it to the unit for counting
        if(subscription_tables[original_vault_id].is_subscribed(addr) || subscription_tables[original_vault_id].is_pending_removal(addr)) {
          val_vault_id = subscription_tables[original_vault_id][addr];
          if(subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::LRU) {
            // Update the LRU entries of the address in from table (located in the original vault) and to table (located in the current subscribed vault)
            lru_units[original_vault_id].touch(addr);
            lru_units[val_vault_id].touch(addr);
          } else if(subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::LFU) {
            // Update the LFU entries of the address in from table (located in the original vault) and to table (located in the current subscribed vault)
            lfu_units[original_vault_id].touch(addr);
            lfu_units[val_vault_id].touch(addr);
          } else if(subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::DirtyLFU) {
            assert(false);
          }
          print_debug_info("Redirecting "+to_string(addr)+" to vault "+to_string(subscription_tables[original_vault_id][addr])+" because it is subscribed");
          // Also, we set the address vector's vault to the subscribed vault so it can be sent to the correct vault for processing.
          req.addr_vec[int(HMC::Level::Vault)] = val_vault_id;
        }
        // Calculate hops and count for prefetch policy check and implementation
        uint64_t hops = (uint64_t)calculate_hops_travelled(req_vault_id, val_vault_id);
        uint64_t count = count_table.update_counter_table_and_get_count(req_vault_id, addr);
        // If the policy says that we should subscribe this address, we subscribe it to the requester's vault so it is closer when accessed in the future
        // We do not subscribe in the case that the original vault has the address pending subscription or removal, or when the subscription is already done to the requester vault
        if(check_prefetch(hops, count)) {
          print_debug_info("Checking subscription at vault "+to_string(req_vault_id)+" and address "+to_string(addr));
          print_debug_info("Is pending subscription? "+to_string(subscription_tables[req_vault_id].is_pending_subscription(addr)));
          print_debug_info("Is pending removal? "+to_string(subscription_tables[req_vault_id].is_pending_removal(addr)));
          print_debug_info("Is subscribed to "+to_string(req_vault_id)+"?"+to_string(subscription_tables[req_vault_id].is_subscribed(addr, req_vault_id)));
        }
        if(check_prefetch(hops, count) && !(subscription_tables[req_vault_id].is_pending_subscription(addr) || subscription_tables[req_vault_id].is_pending_removal(addr) || subscription_tables[req_vault_id].is_subscribed(addr, req_vault_id))) {
          // cout << "Address " << addr << " with hop " << hops << " and count " << count << " and originally in vault " << original_vault_id << " meets subscription threshold. We now subscribe it from " << val_vault_id << " to " << req_vault_id << endl;
          subscribe_address(req_vault_id, addr);
        }
        total_memory_accesses++;
      }
      void print_stats(){
        cout << "-----Prefetcher Stats-----" << endl;
        cout << "Total memory accesses: " << total_memory_accesses << endl;
        cout << "Total submitted subscriptions: " << total_submitted_subscriptions << endl;
        cout << "Total Successful Subscription: " << total_successful_subscriptions << endl;
        cout << "Total Unsuccessful Subscription: " << total_unsuccessful_subscriptions << endl;
        cout << "Total Successful Subscription from Subscription Buffer: " << total_subscription_from_buffer << endl;
        cout << "Total Unsubscription: " << total_unsubscriptions << endl;
        cout << "Total Resubscription: " << total_resubscriptions << endl;
        cout << "Total Successful Insertation into the Subscription Buffer: " << total_buffer_successful_insertation << endl;
        cout << "Total Unsuccessful Insertation into the Subscription Buffer: " << total_buffer_unsuccessful_insertation << endl;
        count_table.print_stats();
      }
    };
    
    SubscriptionPrefetcherSet prefetcher_set;

    vector<int> addr_bits;
    vector<vector <int> > address_distribution;

    int tx_bits;

    Memory(const Config& configs, vector<Controller<HMC>*> ctrls)
        : ctrls(ctrls),
          spec(ctrls[0]->channel->spec),
          addr_bits(int(HMC::Level::MAX)),
          prefetcher_set(ctrls.size(), this)
    {
        // make sure 2^N channels/ranks
        // TODO support channel number that is not powers of 2
        int *sz = spec->org_entry.count;
        assert((sz[0] & (sz[0] - 1)) == 0);
        assert((sz[1] & (sz[1] - 1)) == 0);
        // validate size of one transaction
        int tx = (spec->prefetch_size * spec->channel_width / 8);
        tx_bits = calc_log2(tx);
        assert((1<<tx_bits) == tx);

        pim_mode_enabled = configs.pim_mode_enabled();
        network_overhead = configs.network_overhead_enabled();

        capacity_per_stack = spec->channel_width / 8;

        for (unsigned int lev = 0; lev < addr_bits.size(); lev++) {
          addr_bits[lev] = calc_log2(sz[lev]);
          capacity_per_stack *= sz[lev];
        }
        max_address = capacity_per_stack * configs.get_stacks();

        addr_bits[int(HMC::Level::MAX) - 1] -= calc_log2(spec->prefetch_size);

        // Initiating translation
        if (configs.contains("translation")) {
          translation = name_to_translation[configs["translation"]];
        }
        if (translation != Translation::None) {
          // construct a list of available pages
          // TODO: this should not assume a 4KB page!
          free_physical_pages_remaining = max_address >> 12;

          free_physical_pages.resize(free_physical_pages_remaining, -1);
        }

        // Initiating addressing
        if (configs.contains("addressing_type")) {
          assert(name_to_type.find(configs["addressing_type"]) != name_to_type.end());
          printf("configs[\"addressing_type\"] %s\n", configs["addressing_type"].c_str());
          type = name_to_type[configs["addressing_type"]];
        }

        // HMC
        assert(spec->source_links > 0);
        tags_pools.resize(spec->source_links);
        for (auto & tags_pool : tags_pools) {
          for (int i = 0 ; i < spec->max_tags ; ++i) {
            tags_pool.push_back(i);
          }
        }

        int stacks = configs.get_int_value("stacks");
        for (int i = 0 ; i < stacks ; ++i) {
          logic_layers.emplace_back(new LogicLayer<HMC>(configs, i, spec, ctrls,
              this, std::bind(&Memory<HMC>::receive_packets, this,
                              std::placeholders::_1)));
        }

        cout << "Request type = "<< int(Request::Type::READ) << " is a read \n";
        cout << "Request type = " << int(Request::Type::WRITE) << " is a write \n";

        num_cores = configs.get_core_num();
        cout << "Number of cores in HMC Memory: " << configs.get_core_num() << endl;
        address_distribution.resize(configs.get_core_num());
        for(int i=0; i < configs.get_core_num(); i++){
            //up to 32 vaults
            address_distribution[i].resize(32);
            for(int j=0; j < 32; j++){
                address_distribution[i][j] = 0;
            }
        }

        this -> set_application_name(configs.get_application_name());
        if(configs.get_record_memory_trace()){
          this -> set_address_recorder();
        }

        if (configs.contains("subscription_prefetcher")) {
          cout << "Using prefetcher: " << configs["subscription_prefetcher"] << endl;
          subscription_prefetcher_type = name_to_prefetcher_type[configs["subscription_prefetcher"]];
          if(subscription_prefetcher_type == SubscriptionPrefetcherType::Allocate) {
            prefetcher_set.set_swap_switch(false);
          } else if(subscription_prefetcher_type == SubscriptionPrefetcherType::Swap) {
            prefetcher_set.set_swap_switch(true);
          }
        }

        if (configs.contains("prefetcher_count_threshold")) {
          prefetcher_set.set_prefetch_count_threshold(stoi(configs["prefetcher_count_threshold"]));
        }

        if (configs.contains("prefetcher_hops_threshold")) {
          prefetcher_set.set_prefetch_hops_threshold(stoi(configs["prefetcher_hops_threshold"]));
        }

        if (configs.contains("prefetcher_subscription_table_size")) {
          prefetcher_set.set_subscription_table_size(stoi(configs["prefetcher_subscription_table_size"]));
        }

        if (configs.contains("prefetcher_subscription_table_way")) {
          prefetcher_set.set_subscription_table_ways(stoi(configs["prefetcher_subscription_table_way"]));
        }

        if (configs.contains("prefetcher_subscription_buffer_size")) {
          prefetcher_set.set_subscription_buffer_size(stoi(configs["prefetcher_subscription_buffer_size"]));
        }

        if (configs.contains("prefetcher_table_replacement_policy")) {
          prefetcher_set.set_subscription_table_replacement_policy(configs["prefetcher_table_replacement_policy"]);
        }

        if (configs.contains("prefetcher_count_table_size")) {
          prefetcher_set.set_counter_table_size(stoi(configs["prefetcher_count_table_size"]));
        }

        if (configs.contains("print_debug_info")) {
          prefetcher_set.set_debug_flag(configs["print_debug_info"] == "true");
        }

        if (subscription_prefetcher_type != SubscriptionPrefetcherType::None) {
          prefetcher_set.initialize_sets();
        }
        max_block_col_bits = spec->maxblock_entry.flit_num_bits - tx_bits;
        cout << "maxblock_entry.flit_num_bits: " << spec->maxblock_entry.flit_num_bits << " tx_bits: " << tx_bits << " max_block_col_bits: " << max_block_col_bits << endl;

        // regStats
        dram_capacity
            .name("dram_capacity")
            .desc("Number of bytes in simulated DRAM")
            .precision(0)
            ;
        dram_capacity = max_address;

        num_dram_cycles
            .name("dram_cycles")
            .desc("Number of DRAM cycles simulated")
            .precision(0)
            ;

        num_read_requests
            .init(configs.get_core_num())
            .name("read_requests")
            .desc("Number of incoming read requests to DRAM")
            .precision(0)
            ;

        num_write_requests
            .init(configs.get_core_num())
            .name("write_requests")
            .desc("Number of incoming write requests to DRAM")
            .precision(0)
            ;

        incoming_requests_per_channel
            .init(sz[int(HMC::Level::Vault)])
            .name("incoming_requests_per_channel")
            .desc("Number of incoming requests to each DRAM channel")
            .precision(0)
            ;

        incoming_read_reqs_per_channel
            .init(sz[int(HMC::Level::Vault)])
            .name("incoming_read_reqs_per_channel")
            .desc("Number of incoming read requests to each DRAM channel")
            .precision(0)
            ;
        ramulator_active_cycles
            .name("ramulator_active_cycles")
            .desc("The total number of cycles that the DRAM part is active (serving R/W)")
            .precision(0)
            ;
        memory_footprint
            .name("memory_footprint")
            .desc("memory footprint in byte")
            .precision(0)
            ;
        physical_page_replacement
            .name("physical_page_replacement")
            .desc("The number of times that physical page replacement happens.")
            .precision(0)
            ;

        maximum_internal_bandwidth
            .name("maximum_internal_bandwidth")
            .desc("The theoretical maximum bandwidth (Bps)")
            .precision(0)
            ;

        maximum_link_bandwidth
            .name("maximum_link_bandwidth")
            .desc("The theoretical maximum bandwidth (Bps)")
            .precision(0)
            ;

        read_bandwidth
            .name("read_bandwidth")
            .desc("Real read bandwidth(Bps)")
            .precision(0)
            ;

        write_bandwidth
            .name("write_bandwidth")
            .desc("Real write bandwidth(Bps)")
            .precision(0)
            ;
        read_latency_sum
            .name("read_latency_sum")
            .desc("The memory latency cycles (in memory time domain) sum for all read requests")
            .precision(0)
            ;
        read_latency_avg
            .name("read_latency_avg")
            .desc("The average memory latency cycles (in memory time domain) per request for all read requests")
            .precision(6)
            ;
        queueing_latency_sum
            .name("queueing_latency_sum")
            .desc("The sum of time waiting in queue before first command issued")
            .precision(0)
            ;
        queueing_latency_avg
            .name("queueing_latency_avg")
            .desc("The average of time waiting in queue before first command issued")
            .precision(6)
            ;
        read_latency_ns_avg
            .name("read_latency_ns_avg")
            .desc("The average memory latency (ns) per request for all read requests in this channel")
            .precision(6)
            ;
        queueing_latency_ns_avg
            .name("queueing_latency_ns_avg")
            .desc("The average of time (ns) waiting in queue before first command issued")
            .precision(6)
            ;
        request_packet_latency_sum
            .name("request_packet_latency_sum")
            .desc("The memory latency cycles (in memory time domain) sum for all read request packets transmission")
            .precision(0)
            ;
        request_packet_latency_avg
            .name("request_packet_latency_avg")
            .desc("The average memory latency cycles (in memory time domain) per request for all read request packets transmission")
            .precision(6)
            ;
        request_packet_latency_ns_avg
            .name("request_packet_latency_ns_avg")
            .desc("The average memory latency (ns) per request for all read request packets transmission")
            .precision(6)
            ;
        response_packet_latency_sum
            .name("response_packet_latency_sum")
            .desc("The memory latency cycles (in memory time domain) sum for all read response packets transmission")
            .precision(0)
            ;
        response_packet_latency_avg
            .name("response_packet_latency_avg")
            .desc("The average memory latency cycles (in memory time domain) per response for all read response packets transmission")
            .precision(6)
            ;
        response_packet_latency_ns_avg
            .name("response_packet_latency_ns_avg")
            .desc("The average memory latency (ns) per response for all read response packets transmission")
            .precision(6)
            ;

        // shared by all Controller objects

        read_transaction_bytes
            .name("read_transaction_bytes")
            .desc("The total byte of read transaction")
            .precision(0)
            ;
        write_transaction_bytes
            .name("write_transaction_bytes")
            .desc("The total byte of write transaction")
            .precision(0)
            ;

        row_hits
            .name("row_hits")
            .desc("Number of row hits")
            .precision(0)
            ;
        row_misses
            .name("row_misses")
            .desc("Number of row misses")
            .precision(0)
            ;
        row_conflicts
            .name("row_conflicts")
            .desc("Number of row conflicts")
            .precision(0)
            ;

        read_row_hits
            .init(configs.get_core_num())
            .name("read_row_hits")
            .desc("Number of row hits for read requests")
            .precision(0)
            ;
        read_row_misses
            .init(configs.get_core_num())
            .name("read_row_misses")
            .desc("Number of row misses for read requests")
            .precision(0)
            ;
        read_row_conflicts
            .init(configs.get_core_num())
            .name("read_row_conflicts")
            .desc("Number of row conflicts for read requests")
            .precision(0)
            ;

        write_row_hits
            .init(configs.get_core_num())
            .name("write_row_hits")
            .desc("Number of row hits for write requests")
            .precision(0)
            ;
        write_row_misses
            .init(configs.get_core_num())
            .name("write_row_misses")
            .desc("Number of row misses for write requests")
            .precision(0)
            ;
        write_row_conflicts
            .init(configs.get_core_num())
            .name("write_row_conflicts")
            .desc("Number of row conflicts for write requests")
            .precision(0)
            ;

        req_queue_length_sum
            .name("req_queue_length_sum")
            .desc("Sum of read and write queue length per memory cycle.")
            .precision(0)
            ;
        req_queue_length_avg
            .name("req_queue_length_avg")
            .desc("Average of read and write queue length per memory cycle.")
            .precision(6)
            ;

        read_req_queue_length_sum
            .name("read_req_queue_length_sum")
            .desc("Read queue length sum per memory cycle.")
            .precision(0)
            ;
        read_req_queue_length_avg
            .name("read_req_queue_length_avg")
            .desc("Read queue length average per memory cycle.")
            .precision(6)
            ;

        write_req_queue_length_sum
            .name("write_req_queue_length_sum")
            .desc("Write queue length sum per memory cycle.")
            .precision(0)
            ;
        write_req_queue_length_avg
            .name("write_req_queue_length_avg")
            .desc("Write queue length average per memory cycle.")
            .precision(6)
            ;

        record_read_hits
            .init(configs.get_core_num())
            .name("record_read_hits")
            .desc("record read hit count for this core when it reaches request limit or to the end")
            ;

        record_read_misses
            .init(configs.get_core_num())
            .name("record_read_misses")
            .desc("record_read_miss count for this core when it reaches request limit or to the end")
            ;

        record_read_conflicts
            .init(configs.get_core_num())
            .name("record_read_conflicts")
            .desc("record read conflict count for this core when it reaches request limit or to the end")
            ;

        record_write_hits
            .init(configs.get_core_num())
            .name("record_write_hits")
            .desc("record write hit count for this core when it reaches request limit or to the end")
            ;

        record_write_misses
            .init(configs.get_core_num())
            .name("record_write_misses")
            .desc("record write miss count for this core when it reaches request limit or to the end")
            ;

        record_write_conflicts
            .init(configs.get_core_num())
            .name("record_write_conflicts")
            .desc("record write conflict for this core when it reaches request limit or to the end")
            ;

        for (auto ctrl : ctrls) {
          ctrl->read_transaction_bytes = &read_transaction_bytes;
          ctrl->write_transaction_bytes = &write_transaction_bytes;

          ctrl->row_hits = &row_hits;
          ctrl->row_misses = &row_misses;
          ctrl->row_conflicts = &row_conflicts;
          ctrl->read_row_hits = &read_row_hits;
          ctrl->read_row_misses = &read_row_misses;
          ctrl->read_row_conflicts = &read_row_conflicts;
          ctrl->write_row_hits = &write_row_hits;
          ctrl->write_row_misses = &write_row_misses;
          ctrl->write_row_conflicts = &write_row_conflicts;

          ctrl->queueing_latency_sum = &queueing_latency_sum;

          ctrl->req_queue_length_sum = &req_queue_length_sum;
          ctrl->read_req_queue_length_sum = &read_req_queue_length_sum;
          ctrl->write_req_queue_length_sum = &write_req_queue_length_sum;

          ctrl->record_read_hits = &record_read_hits;
          ctrl->record_read_misses = &record_read_misses;
          ctrl->record_read_conflicts = &record_read_conflicts;
          ctrl->record_write_hits = &record_write_hits;
          ctrl->record_write_misses = &record_write_misses;
          ctrl->record_write_conflicts = &record_write_conflicts;
        }
    }

    ~Memory()
    {
        for (auto ctrl: ctrls)
            delete ctrl;
        delete spec;
    }

    double clk_ns()
    {
        return spec->speed_entry.tCK;
    }

    void record_core(int coreid) {
      // TODO record multicore statistics
    }

    void tick()
    {
        clk++;
        num_dram_cycles++;

        bool is_active = false;
        for (auto ctrl : ctrls) {
          is_active = is_active || ctrl->is_active();
          ctrl->tick();
        }
        if (is_active) {
          ramulator_active_cycles++;
        }
        for (auto logic_layer : logic_layers) {
          logic_layer->tick();
        }
        if (subscription_prefetcher_type != SubscriptionPrefetcherType::None) {
          prefetcher_set.tick();
        }
    }

    int assign_tag(int slid) {
      if (tags_pools[slid].empty()) {
        return -1;
      } else {
        int tag = tags_pools[slid].front();
        tags_pools[slid].pop_front();
        return tag;
      }
    }

    Packet form_request_packet(const Request& req) {
      // All packets sent from host controller are Request packets

      //cout << "Forming request packet with addr " << req.addr << endl;
      long addr = req.addr;
      int cub = addr / capacity_per_stack;
      long adrs = addr;
      int max_block_bits = spec->maxblock_entry.flit_num_bits;
      clear_lower_bits(addr, max_block_bits);
      int slid = addr % spec->source_links;
      int tag = assign_tag(slid); // may return -1 when no available tag // TODO recycle tags when request callback
      int lng = req.type == Request::Type::READ ?
                                                1 : 1 +  spec->payload_flits;
      Packet::Command cmd;
      switch (int(req.type)) {
        case int(Request::Type::READ):
          cmd = read_cmd_map[lng];
        break;
        case int(Request::Type::WRITE):
          cmd = write_cmd_map[lng];
        break;
        default: assert(false);
      }
      Packet packet(Packet::Type::REQUEST, cub, adrs, tag, lng, slid, cmd);
      packet.req = req;
      
      //cout << "Forming a packet to send to memory \n";
      //cout << "ADDR: " << packet.header.ADRS.value << " CUB " << packet.header.CUB.value << " SLID " << packet.tail.SLID.value << " TAG " << packet.header.TAG.value << " LNG " << lng << endl;

      debug_hmc("cub: %d", cub);
      debug_hmc("adrs: %lx", adrs);
      debug_hmc("slid: %d", slid);
      debug_hmc("lng: %d", lng);
      debug_hmc("cmd: %d", int(cmd));
      // DEBUG:
      assert(packet.header.CUB.valid());
      assert(packet.header.ADRS.valid());
      assert(packet.header.TAG.valid()); // -1 also considered valid here...
      assert(packet.tail.SLID.valid());
      assert(packet.header.CMD.valid());
      return packet;
    }

    void receive_packets(Packet packet) {
      debug_hmc("receive response packets@host controller");
      if (packet.flow_control) {
        return;
      }

      assert(packet.type == Packet::Type::RESPONSE);

      tags_pools[packet.header.SLID.value].push_back(packet.header.TAG.value);
      Request& req = packet.req;
      req.depart_hmc = clk;
      if (req.type == Request::Type::READ) {
        read_latency_sum += req.depart_hmc - req.arrive_hmc;
        debug_hmc("read_latency: %ld", req.depart_hmc - req.arrive_hmc);
        request_packet_latency_sum += req.arrive - req.arrive_hmc;
        debug_hmc("request_packet_latency: %ld", req.arrive - req.arrive_hmc);
        response_packet_latency_sum += req.depart_hmc - req.depart;
        debug_hmc("response_packet_latency: %ld", req.depart_hmc - req.depart);

        req.callback(req);


      }
      else if(req.type == Request::Type::WRITE){
        req.callback(req);
      }
    }

    long address_vector_to_address(const vector<int>& addr_vec) {
      long addr = 0;
      long vault = addr_vec[int(HMC::Level::Vault)];
      long bank_group = addr_vec[int(HMC::Level::BankGroup)];
      long bank = addr_vec[int(HMC::Level::Bank)];
      long row = addr_vec[int(HMC::Level::Row)];
      long column = addr_vec[int(HMC::Level::Column)];
      // cout << "Address Vector is in Vault " << addr_vec[int(HMC::Level::Vault)] << " BankGroup " << addr_vec[int(HMC::Level::BankGroup)]
      //   << " Bank " << addr_vec[int(HMC::Level::Bank)] << " Row " << addr_vec[int(HMC::Level::Row)] << " Column " << addr_vec[int(HMC::Level::Column)];
      int column_significant_bits = addr_bits[int(HMC::Level::Column)] - max_block_col_bits;
      switch(int(type)) {
        case int(Type::RoCoBaVa): {
          addr |= row;
          addr <<= column_significant_bits;
          addr |= (column >> max_block_col_bits);
          addr <<= addr_bits[int(HMC::Level::BankGroup)];
          addr |= bank_group;
          addr <<= addr_bits[int(HMC::Level::Bank)];
          addr |= bank;
          addr <<= addr_bits[int(HMC::Level::Vault)];
          addr |= vault;
          addr <<= max_block_col_bits;
          addr |= column & ((1<<max_block_col_bits) - 1);
        }
        break;
        case int(Type::RoBaCoVa): {
          addr |= row;
          addr <<= addr_bits[int(HMC::Level::BankGroup)];
          addr |= bank_group;
          addr <<= addr_bits[int(HMC::Level::Bank)];
          addr |= bank;
          addr <<= column_significant_bits;
          addr |= (column >> max_block_col_bits);
          addr <<= addr_bits[int(HMC::Level::Vault)];
          addr |= vault;
          addr <<= max_block_col_bits;
          addr |= column & ((1<<max_block_col_bits) - 1);
        }
        break;
        case int(Type::RoCoBaBgVa): {
          addr |= row;
          addr <<= column_significant_bits;
          addr |= (column >> max_block_col_bits);
          addr <<= addr_bits[int(HMC::Level::Bank)];
          addr |= bank;
          addr <<= addr_bits[int(HMC::Level::BankGroup)];
          addr |= bank_group;
          addr <<= addr_bits[int(HMC::Level::Vault)];
          addr |= vault;
          addr <<= max_block_col_bits;
          addr |= column & ((1<<max_block_col_bits) - 1);
        }
        break;
        default:
            assert(false);
      }
      // cout << " and after translation, the original address is: " << addr << endl;
      return addr;
    }

    vector<int> address_to_address_vector(const long& addr) {
      long local_addr = addr;
      // cout << "The input address is " << addr;
      vector<int> addr_vec;
      addr_vec.resize(addr_bits.size());
      switch(int(type)) {
          case int(Type::RoCoBaVa): {
            addr_vec[int(HMC::Level::Column)] =
                slice_lower_bits(local_addr, max_block_col_bits);
            addr_vec[int(HMC::Level::Vault)] =
                slice_lower_bits(local_addr, addr_bits[int(HMC::Level::Vault)]);
            addr_vec[int(HMC::Level::Bank)] =
                slice_lower_bits(local_addr, addr_bits[int(HMC::Level::Bank)]);
            addr_vec[int(HMC::Level::BankGroup)] =
                slice_lower_bits(local_addr, addr_bits[int(HMC::Level::BankGroup)]);
            int column_MSB_bits =
              slice_lower_bits(
                  local_addr, addr_bits[int(HMC::Level::Column)] - max_block_col_bits);
            addr_vec[int(HMC::Level::Column)] =
              addr_vec[int(HMC::Level::Column)] | (column_MSB_bits << max_block_col_bits);
            addr_vec[int(HMC::Level::Row)] =
                slice_lower_bits(local_addr, addr_bits[int(HMC::Level::Row)]);
          }
          break;
          case int(Type::RoBaCoVa): {
            addr_vec[int(HMC::Level::Column)] =
                slice_lower_bits(local_addr, max_block_col_bits);
            addr_vec[int(HMC::Level::Vault)] =
                slice_lower_bits(local_addr, addr_bits[int(HMC::Level::Vault)]);
            int column_MSB_bits =
              slice_lower_bits(
                  local_addr, addr_bits[int(HMC::Level::Column)] - max_block_col_bits);
            addr_vec[int(HMC::Level::Column)] =
              addr_vec[int(HMC::Level::Column)] | (column_MSB_bits << max_block_col_bits);
            addr_vec[int(HMC::Level::Bank)] =
                slice_lower_bits(local_addr, addr_bits[int(HMC::Level::Bank)]);
            addr_vec[int(HMC::Level::BankGroup)] =
                slice_lower_bits(local_addr, addr_bits[int(HMC::Level::BankGroup)]);
            addr_vec[int(HMC::Level::Row)] =
                slice_lower_bits(local_addr, addr_bits[int(HMC::Level::Row)]);
          }
          break;
          case int(Type::RoCoBaBgVa): {
            addr_vec[int(HMC::Level::Column)] =
                slice_lower_bits(local_addr, max_block_col_bits);
            addr_vec[int(HMC::Level::Vault)] =
                slice_lower_bits(local_addr, addr_bits[int(HMC::Level::Vault)]);
            addr_vec[int(HMC::Level::BankGroup)] =
                slice_lower_bits(local_addr, addr_bits[int(HMC::Level::BankGroup)]);
            addr_vec[int(HMC::Level::Bank)] =
                slice_lower_bits(local_addr, addr_bits[int(HMC::Level::Bank)]);
            int column_MSB_bits =
              slice_lower_bits(
                  local_addr, addr_bits[int(HMC::Level::Column)] - max_block_col_bits);
            addr_vec[int(HMC::Level::Column)] =
              addr_vec[int(HMC::Level::Column)] | (column_MSB_bits << max_block_col_bits);
            addr_vec[int(HMC::Level::Row)] =
                slice_lower_bits(local_addr, addr_bits[int(HMC::Level::Row)]);
          }
          break;
          default:
              assert(false);
        }
        // cout << " And after translation, it is in Vault " << addr_vec[int(HMC::Level::Vault)] << " BankGroup " << addr_vec[int(HMC::Level::BankGroup)]
        //     << " Bank " << addr_vec[int(HMC::Level::Bank)] << " Row " << addr_vec[int(HMC::Level::Row)] << " Column " << addr_vec[int(HMC::Level::Column)] << endl;
        return addr_vec;
    }

    bool send(Request req)
    {
      //  cout << "receive request packets@host controller with address " << req.addr << endl;
        req._addr = req.addr;
        req.reqid = mem_req_count;

        // cout << "Address before bit operation is " << bitset<64>(req.addr) << endl;
        clear_higher_bits(req.addr, max_address-1ll);
        // cout << "Address after clear higher bits is" << bitset<64>(req.addr) << endl;
        long addr = req.addr;
        long coreid = req.coreid;

        // Each transaction size is 2^tx_bits, so first clear the lowest tx_bits bits
        clear_lower_bits(addr, tx_bits);
        // cout << "Address after clear lower bits is " << bitset<64>(addr) << endl;
        vector<int> addr_vec = address_to_address_vector(addr);
        // assert(address_vector_to_address(addr_vec) == addr); // Test script to make sure the implementation is correct.
        req.addr_vec = addr_vec;
        int original_vault = req.addr_vec[int(HMC::Level::Vault)];
        int subscribed_vault = original_vault;
        if (subscription_prefetcher_type != SubscriptionPrefetcherType::None) {
          prefetcher_set.access_address(req);
        }
        int requester_vault = req.coreid;

        req.arrive_hmc = clk;

        if(pim_mode_enabled){
            // To model NOC traffic
            //I'm considering 32 vaults. So the 2D mesh will be 36x36
            //To calculate how many hops, check the manhattan distance
            int hops;
            if(!network_overhead) {
              hops = 0;
            }
            else if (req.type == Request::Type::READ){
              // If we do not use prefetcher, we calculate hops the traditional way
              if(subscription_prefetcher_type == SubscriptionPrefetcherType::None){
                // Let's assume 1 Flit = 128 bytes
                // A read request is 64 bytes
                // One read request will take = 1 Flit*hops + 5*hops
                hops = calculate_hops_travelled(requester_vault, subscribed_vault, READ_LENGTH);
              } else {
                hops = 0;
                // We first check the subscription table of the original vault
                hops += calculate_hops_travelled(requester_vault, original_vault);
                // Then the original vault forward this request to the subscribed vault
                hops += calculate_hops_travelled(original_vault, subscribed_vault);
                // Then the subscribed vault send the data back to the requester vault
                hops += calculate_hops_travelled(subscribed_vault, requester_vault)*WRITE_LENGTH;
              }
            }
            else if (req.type == Request::Type::WRITE){
              if(subscription_prefetcher_type == SubscriptionPrefetcherType::None) {
                hops = calculate_hops_travelled(requester_vault, subscribed_vault, WRITE_LENGTH);
              } else {
                hops = 0;
                // We first check the subscription table of the original vault and receive information from it
                hops += calculate_hops_travelled(requester_vault, original_vault)*2;
                // Then we write the data to the subscribed vault
                hops += calculate_hops_travelled(requester_vault, subscribed_vault)*WRITE_LENGTH;
              }
            } else {
              if(subscription_prefetcher_type == SubscriptionPrefetcherType::None) {
                hops = calculate_hops_travelled(requester_vault, subscribed_vault, OTHER_LENGTH);
              } else {
                // The requester first send data to the original vault, then the original vault forward it to the subscribed vault
                hops = calculate_hops_travelled(requester_vault, original_vault)+calculate_hops_travelled(original_vault, subscribed_vault);
              }
            }
            if(network_overhead) {
              total_hops += calculate_hops_travelled(requester_vault, subscribed_vault, OTHER_LENGTH);
            }
            req.hops = hops;

            if(!ctrls[req.addr_vec[int(HMC::Level::Vault)]] -> receive(req)){
              cout << "We are not able to send request with address " << req.addr << endl;
              return false;
            }

            if (req.type == Request::Type::READ) {
                ++num_read_requests[coreid];
                ++incoming_read_reqs_per_channel[req.addr_vec[int(HMC::Level::Vault)]];
            }
            if (req.type == Request::Type::WRITE) {
                ++num_write_requests[coreid];
            }
            ++incoming_requests_per_channel[req.addr_vec[int(HMC::Level::Vault)]];
            ++mem_req_count;

            if(req.coreid >= 0 && req.coreid < 256)
              address_distribution[req.coreid][req.addr_vec[int(HMC::Level::Vault)]]++;
            else
              cerr << "HMC MEMORY: INVALID CORE ID: " << req.coreid << "endl";

            if(get_memory_addresses){
              if (profile_this_epoach){
                memory_addresses << clk << " " << req.addr << " ";
                if (req.type == Request::Type::WRITE)       memory_addresses << "W ";
                else if (req.type == Request::Type::READ)   memory_addresses << "R ";
                else                                        memory_addresses << "NA ";
                memory_addresses << req.addr_vec[int(HMC::Level::Vault)] << " " << req.addr_vec[int(HMC::Level::BankGroup)] << " "
                                 << req.addr_vec[int(HMC::Level::Bank)] << " "  << req.addr_vec[int(HMC::Level::Row)]       << " "
                                 << req.addr_vec[int(HMC::Level::Column)] << "\n";

                instruction_counter++;
                if(instruction_counter >= 10000){
                  profile_this_epoach = false;
                  instruction_counter = 0;
                }
              }
              else{
                instruction_counter++;
                if(instruction_counter >= 990000){
                  profile_this_epoach = true;
                  instruction_counter = 0;
                }
              }
            }

            memory_addresses << clk << " " << req.addr << " ";
            if (req.type == Request::Type::WRITE)       memory_addresses << "W ";
            else if (req.type == Request::Type::READ)   memory_addresses << "R ";
            else                                        memory_addresses << "NA ";
            memory_addresses << req.addr_vec[int(HMC::Level::Vault)] << " " << req.addr_vec[int(HMC::Level::BankGroup)] << " "
                             << req.addr_vec[int(HMC::Level::Bank)] << " "  << req.addr_vec[int(HMC::Level::Row)]       << " "
                             << req.addr_vec[int(HMC::Level::Column)] << "\n";

            return true;
        }
        else{
            Packet packet = form_request_packet(req);
            if (packet.header.TAG.value == -1) {
                return false;
            }

            // TODO support multiple stacks
            Link<HMC>* link =
                logic_layers[0]->host_links[packet.tail.SLID.value].get();
            if (packet.total_flits <= link->slave.available_space()) {
              link->slave.receive(packet);
              if (req.type == Request::Type::READ) {
                ++num_read_requests[coreid];
                ++incoming_read_reqs_per_channel[req.addr_vec[int(HMC::Level::Vault)]];
              }
              if (req.type == Request::Type::WRITE) {
                ++num_write_requests[coreid];
              }
              ++incoming_requests_per_channel[req.addr_vec[int(HMC::Level::Vault)]];
              ++mem_req_count;

              if(req.coreid >= 0 && req.coreid < 256)
                address_distribution[req.coreid][req.addr_vec[int(HMC::Level::Vault)]]++;
              else
                cerr << "HMC MEMORY: INVALID CORE ID: " << req.coreid << "endl";
              return true;
            }
            else {
              return false;
            }
        }

        if(get_memory_addresses){
          cout << "Get memory address \n";
          if (profile_this_epoach){

            memory_addresses << clk << " " << req.addr << " ";
            if (req.type == Request::Type::WRITE)       memory_addresses << "W ";
            else if (req.type == Request::Type::READ)   memory_addresses << "R ";
            else                                        memory_addresses << "NA ";
            memory_addresses << req.addr_vec[int(HMC::Level::Vault)] << " " << req.addr_vec[int(HMC::Level::BankGroup)] << " "
                             << req.addr_vec[int(HMC::Level::Bank)] << " "  << req.addr_vec[int(HMC::Level::Row)]       << " "
                             << req.addr_vec[int(HMC::Level::Column)] << "\n";

            instruction_counter++;
            if(instruction_counter >= 10000){
              profile_this_epoach = false;
              instruction_counter = 0;
            }
          }
          else{
            instruction_counter++;
            if(instruction_counter >= 990000){
              profile_this_epoach = true;
              instruction_counter = 0;
            }
          }
        }

        memory_addresses << clk << " " << req.addr << " ";
        if (req.type == Request::Type::WRITE)       memory_addresses << "W ";
        else if (req.type == Request::Type::READ)   memory_addresses << "R ";
        else                                        memory_addresses << "NA ";
        memory_addresses << req.addr_vec[int(HMC::Level::Vault)] << " " << req.addr_vec[int(HMC::Level::BankGroup)] << " "
                         << req.addr_vec[int(HMC::Level::Bank)] << " "  << req.addr_vec[int(HMC::Level::Row)]       << " "
                         << req.addr_vec[int(HMC::Level::Column)] << "\n";
        return true;
    }

    int pending_requests()
    {
        int reqs = 0;
        for (auto ctrl: ctrls)
            reqs += ctrl->readq.size() + ctrl->writeq.size() + ctrl->otherq.size() + ctrl->pending.size();
        return reqs;
    }

    void finish() {
      std::cout << "[RAMULATOR] Gathering stats \n";

      dram_capacity = max_address;
      int *sz = spec->org_entry.count;
      maximum_internal_bandwidth =
        spec->speed_entry.rate * 1e6 * spec->channel_width * sz[int(HMC::Level::Vault)] / 8;
      maximum_link_bandwidth =
        spec->link_width * 2 * spec->source_links * spec->lane_speed * 1e9 / 8;

      long dram_cycles = num_dram_cycles.value();
      long total_read_req = num_read_requests.total();
      for (auto ctrl : ctrls) {
        ctrl->finish(dram_cycles);
      }
      read_bandwidth = read_transaction_bytes.value() * 1e9 / (dram_cycles * clk_ns());
      write_bandwidth = write_transaction_bytes.value() * 1e9 / (dram_cycles * clk_ns());;
      read_latency_avg = read_latency_sum.value() / total_read_req;
      queueing_latency_avg = queueing_latency_sum.value() / total_read_req;
      request_packet_latency_avg = request_packet_latency_sum.value() / total_read_req;
      response_packet_latency_avg = response_packet_latency_sum.value() / total_read_req;
      read_latency_ns_avg = read_latency_avg.value() * clk_ns();
      queueing_latency_ns_avg = queueing_latency_avg.value() * clk_ns();
      request_packet_latency_ns_avg = request_packet_latency_avg.value() * clk_ns();
      response_packet_latency_ns_avg = response_packet_latency_avg.value() * clk_ns();
      req_queue_length_avg = req_queue_length_sum.value() / dram_cycles;
      read_req_queue_length_avg = read_req_queue_length_sum.value() / dram_cycles;
      write_req_queue_length_avg = write_req_queue_length_sum.value() / dram_cycles;

      string to_open = application_name+".ramulator.address_distribution";
      cout << "Address distribution stored at: " << to_open << endl;
      cout << "Number of cores: " << num_cores << endl;
      std::ofstream ofs(to_open.c_str(), std::ofstream::out);
      ofs << "CoreID VaultID #Requests\n";
      for(int i=0; i < address_distribution.size(); i++){
        for(int j=0; j < 32; j++){
          ofs << i << " " << j << " " <<  address_distribution[i][j] << "\n";
        }
      }
      ofs.close();
      memory_addresses.close();
      if (subscription_prefetcher_type != SubscriptionPrefetcherType::None) {
        prefetcher_set.print_stats();
      }
      cout << "Total number of hops travelled: " << total_hops << endl;
    }

    long page_allocator(long addr, int coreid) {
        long virtual_page_number = addr >> 12;

        switch(int(translation)) {
            case int(Translation::None): {
              return addr;
            }
            case int(Translation::Random): {
                auto target = make_pair(coreid, virtual_page_number);
                if(page_translation.find(target) == page_translation.end()) {
                    // page doesn't exist, so assign a new page
                    // make sure there are physical pages left to be assigned

                    // if physical page doesn't remain, replace a previous assigned
                    // physical page.
                    memory_footprint += 1<<12;
                    if (!free_physical_pages_remaining) {
                      physical_page_replacement++;
                      long phys_page_to_read = lrand() % free_physical_pages.size();
                      assert(free_physical_pages[phys_page_to_read] != -1);
                      page_translation[target] = phys_page_to_read;
                    } else {
                        // assign a new page
                        long phys_page_to_read = lrand() % free_physical_pages.size();
                        // if the randomly-selected page was already assigned
                        if(free_physical_pages[phys_page_to_read] != -1) {
                            long starting_page_of_search = phys_page_to_read;

                            do {
                                // iterate through the list until we find a free page
                                // TODO: does this introduce serious non-randomness?
                                ++phys_page_to_read;
                                phys_page_to_read %= free_physical_pages.size();
                            }
                            while((phys_page_to_read != starting_page_of_search) && free_physical_pages[phys_page_to_read] != -1);
                        }

                        assert(free_physical_pages[phys_page_to_read] == -1);

                        page_translation[target] = phys_page_to_read;
                        free_physical_pages[phys_page_to_read] = coreid;
                        --free_physical_pages_remaining;
                    }
                }

                // SAUGATA TODO: page size should not always be fixed to 4KB
                return (page_translation[target] << 12) | (addr & ((1 << 12) - 1));
            }
            default:
                assert(false);
        }

    }


private:
    int calc_log2(int val){
        int n = 0;
        while ((val >>= 1))
            n ++;
        return n;
    }
    int slice_lower_bits(long& addr, int bits)
    {
        int lbits = addr & ((1<<bits) - 1);
        addr >>= bits;
        return lbits;
    }
    void clear_lower_bits(long& addr, int bits)
    {
        addr >>= bits;
    }
    void clear_higher_bits(long& addr, long mask) {
        addr = (addr & mask);
    }
    long lrand(void) {
        if(sizeof(int) < sizeof(long)) {
            return static_cast<long>(rand()) << (sizeof(int) * 8) | rand();
        }

        return rand();
    }
};

} /*namespace ramulator*/

#endif /*__HMC_MEMORY_H*/
