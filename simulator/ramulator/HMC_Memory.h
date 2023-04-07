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
    };

    // A subscription based prefetcher
    class SubscriptionPrefetcherSet {
    private:
      // Subscription task. Denoting where it is subscribing from, and where to, and how many cycles of latency
      struct SubscriptionTask {
        long addr;
        int original_vault;
        int req_vault;
        int hops;
        SubscriptionTask(long addr, int original_vault, int req_vault, int hops):addr(addr),original_vault(original_vault),req_vault(req_vault),hops(hops){}
        SubscriptionTask(){}
      };

      // The actual subscription table. Translates an address to its subscribed vault
      class SubscriptionTable{
        private:
        bool initialized = false; // Each subscription table can be only initialized once
        int controllers = 1; // How many subscription tables there are, usually the same as the core(vault) number
        // Specs for Subscription table
        size_t subscription_table_size = SIZE_MAX;
        size_t subscription_table_ways = subscription_table_size;
        size_t subscription_table_sets = subscription_table_size / subscription_table_ways;
        // Actual data structure for those tables
        unordered_map<long, int> address_translation_table; // Subscribe remote address (1st val) to local address (2nd address)
        vector<vector<size_t>> virtualized_table_sets; // Used for limiting the number of ways in each "set" in each subscription table
        public:
        SubscriptionTable(){}
        SubscriptionTable(size_t size, size_t ways):subscription_table_size(size),subscription_table_ways(ways) {initialize();} // Only set from table size
        SubscriptionTable(int controllers, size_t size, size_t ways):controllers(controllers),subscription_table_size(size),subscription_table_ways(ways){initialize();}// Set both from and to table specs
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
        void set_controllers(int controllers) {this -> controllers = controllers;}
        int get_controllers() {return controllers;}
        // We split initialize() function from constructor as it might be called after constructor. It can be only exec'ed once
        void initialize(){
          // We can only initialize once
          assert(!initialized);
          // Report the number of vaults first (we are now using 1 table for all vaults so it is always 1)
          cout << "We are simulating " << controllers << " subscription tables." << endl;
          // Initialize the subscription table
          assert(subscription_table_size % subscription_table_ways == 0);
          subscription_table_sets = subscription_table_size / subscription_table_ways;
          cout << "Subscription Table Size: " << (subscription_table_size == SIZE_MAX ? "Unlimited" : to_string(subscription_table_size)) << endl;
          cout << "Subscription Table Ways: " << (subscription_table_ways == SIZE_MAX ? "Unlimited" : to_string(subscription_table_ways)) << endl;
          cout << "Subscription Table Sets: " << subscription_table_sets << endl;
          // One subscription to table per vault
          virtualized_table_sets.assign(controllers, vector<size_t>(subscription_table_sets, 0));

          initialized = true;
        }
        size_t get_set(long addr)const{return addr % subscription_table_sets;}
        bool subscription_table_is_free(int table_vault, long addr) const {return virtualized_table_sets[table_vault][get_set(addr)] < subscription_table_ways;}
        bool can_insert_to_table(int original_vault, int req_vault, long addr)const{return subscription_table_is_free(req_vault, addr) && subscription_table_is_free(original_vault, addr);}
        void immediate_subscribe_address(int original_vault, int req_vault, long addr){
          // cout << "Subscribing address " << addr << " from " << original_vault << " to " << req_vault;
          // cout << " subscription table of vault " << original_vault << " at set " << get_set(addr) << " has entry " << virtualized_table_sets[original_vault][get_set(addr)];
          // cout << " subscription table of vault " << req_vault << " at set " << get_set(addr) << " has entry " << virtualized_table_sets[req_vault][get_set(addr)] << endl;
          address_translation_table[addr] = req_vault; // Subscribe the remote vault to local vault
          virtualized_table_sets[req_vault][get_set(addr)]++; // Increase the "virtual" set's content count for subscription to the destination vault
          virtualized_table_sets[original_vault][get_set(addr)]++; // Increase the "virtual" set's content count for subscription from the original vault
        }
        void immediate_unsubscribe_address(int original_vault, long addr){
          // Actually remove the address from the table
          int current_vault = address_translation_table[addr];
          address_translation_table.erase(addr);

          // Decrease the "virtual" set's content count for subscription from the original vault
          if(virtualized_table_sets[original_vault][get_set(addr)] > 0) {
            virtualized_table_sets[original_vault][get_set(addr)]--;
          }

          // Decrease the "virtual" set's content count for subscription to the destination vault
          if(virtualized_table_sets[current_vault][get_set(addr)] > 0) {
            virtualized_table_sets[current_vault][get_set(addr)]--;
          }
        }
        bool has(long addr) const{return address_translation_table.count(addr) > 0;}
        int& operator[](const long& addr){return address_translation_table[addr];}
        size_t count(long addr) const{return address_translation_table.count(addr);}
      };
      SubscriptionTable subscription_table;

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
        size_t address_access_history_size = SIZE_MAX; // Use for LRU
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
        void touch(long addr){
          // If there exists the address in access history, we first remove it
          if(address_access_history_map.count(addr)) {
            address_access_history[get_set(addr)].erase(address_access_history_map[addr]);
            address_access_history_map.erase(addr);
            address_access_history_used--;
          }
          // Then if the address access history table is still larger than maximum minus one, we make some space
          while(address_access_history_used >= address_access_history_size) {
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
        void erase(long addr){
          if(address_access_history_map.count(addr)){
            address_access_history[get_set(addr)].erase(address_access_history_map[addr]);
            address_access_history_map.erase(addr);
            address_access_history_used--;
          }
        }
        long find_victim(long addr)const{
          assert(initialized);
          return address_access_history[get_set(addr)].back();
        }
      };
      class LFUUnit {
        private:
        bool initialized = false;
        size_t count_priority_queue_size = SIZE_MAX;
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
          LFUPriorityQueueItem item = LFUPriorityQueueItem(addr, 0);
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
          while(count_priority_queue_used >= count_priority_queue_size) {
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
          cout << "Counter table size: " << counter_table_size << endl;
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
        auto operator[](const size_t& i) -> decltype(count_tables[i]){return count_tables[i];}
        void print_stats(){
          cout << "We have inserted " << insertions << " into the counter table " << " and evicted " << evictions << " from it." << endl;
        }
      };
      CountTable count_table;

      // Actually dicatates when prefetch happens.
      uint64_t prefetch_hops_threshold = 5;
      uint64_t prefetch_count_threshold = 1;

      // A pointer so we can easily access Memory members
      Memory<HMC, Controller>* mem_ptr;

      // Variables used for statistic purposes
      long total_memory_accesses = 0;
      long total_submitted_subscriptions = 0;
      long total_successful_subscriptions = 0;
      long total_unsuccessful_subscriptions = 0;
      long total_unsubscriptions = 0;
      long total_buffer_successful_insertation = 0;
      long total_buffer_unsuccessful_insertation = 0;
      long total_subscription_from_buffer = 0;
      long total_unsubscriptions_as_a_result_of_replacement = 0;

      // Tasks in pending_subscription and pending_unsubscription are being communicated via the network
      list<SubscriptionTask> pending_subscription;
      list<SubscriptionTask> pending_unsubscription;

      // Buffer to be used when the subscription table is "full". Tasks in this queue is actually at its destination
      size_t subscription_buffer_size = 32; // Anything too large may take long time (days to weeks) to execute for some benchmarks, it shouldn't be too large either as it's a fully-associative queue
      list<SubscriptionTask> subscription_buffer;
      unordered_map<long, typename list<SubscriptionTask>::iterator> subscription_buffer_map; // To ensure that we don't put two addresses in the same buffer twice

      int controllers; // Record how many vaults we have
      bool swap = true; // Reserved for future use (no swap)
    public:
      SubscriptionPrefetcherSet(int controllers, Memory<HMC, Controller>* mem_ptr):controllers(controllers),mem_ptr(mem_ptr) {
        count_table.set_controllers(controllers);
        subscription_table.set_controllers(controllers);
      }
      int find_original_vault_of_address(long addr){
        vector<int> addr_vec = mem_ptr -> address_to_address_vector(addr);
        return addr_vec[int(HMC::Level::Vault)];
      }
      bool subscription_buffer_is_free(int req_vault) const {return subscription_buffer.size() < subscription_buffer_size;}
      long find_victim_for_unsubscription(int vault, long addr) const {
        long victim_addr = 0;
        if(subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::LRU) {
          victim_addr = lru_units[vault].find_victim(addr); // LRU Logic
        } else if(subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::LFU || subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::DirtyLFU) {
          // cout << "We found victim " << lfu_unit.find_victim(addr) << endl;
          victim_addr = lfu_units[vault].find_victim(addr); // LFU Logic
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
        if(subscription_table.get_subscription_table_ways() == SIZE_MAX){
          subscription_table.set_subscription_table_ways(size);
        }
        subscription_table.set_subscription_table_size(size);
      }
      void set_subscription_table_ways(size_t ways) {subscription_table.set_subscription_table_ways(ways);}
      void set_subscription_buffer_size(size_t size) {subscription_buffer_size = size;}
      void set_subscription_table_replacement_policy(string policy) {
        subscription_table_replacement_policy = name_to_prefetcher_rp[policy];
        cout << "Subscription table replacement policy is: " << policy << endl;
      }
      void initialize_sets(){
        subscription_table.initialize();
        count_table.initialize();
        if(subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::LRU) {
          lru_units.assign(controllers, LRUUnit(subscription_table.get_subscription_table_sets()));
        } else if(subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::LFU) {
          lfu_units.assign(controllers, LFUUnit(subscription_table.get_subscription_table_sets()));
        } else if(subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::DirtyLFU) {
          cout << "DirtyLFU is no longer supported due to it causing starving in the buffer" << endl;
          assert(false);
        } else {
          cout << "Unknown replacement policy!" << endl;
          assert(false); // We fail early if the policy is not known.
        }
        cout << "Subscription buffer size: " << subscription_buffer_size << endl;
      }
      bool check_prefetch(uint64_t hops, uint64_t count) const {
        // TODO: Use machine learning to optimize prefetching
        return hops >= prefetch_hops_threshold && count >= prefetch_count_threshold;
      }
      void unsubscribe_address(long addr) {
        if(subscription_table.has(addr) == 0) {
            // cout << "Address " << addr << " does not exist in the subscription table" << endl;
            return; // If there is no local record, do nothing.
        }
        // Calculate the address vector based on the address
        vector<int> addr_vec = mem_ptr -> address_to_address_vector(addr);
        // Calculate "hops" - it is actually the number of cycles it takes for the unsubscribe request to finish
        int hops = calculate_hops_travelled(addr_vec[int(HMC::Level::Vault)], subscription_table[addr], WRITE_LENGTH);
        // We find the "victim vault" so we can swap it back. the definition is below in subscribe_address()
        vector<int> victim_vec(addr_vec);
        victim_vec[int(HMC::Level::Vault)] = subscription_table[addr]; // We find the original page to swap back
        long victim_addr = mem_ptr -> address_vector_to_address(victim_vec);
        // cout << "Submitting address " << addr << " for unsubscription. It will take " << hops << " cycles" << endl;
        // Submit the address for unsubscription. If the victim address exists, we unsubscribe it too.
        submit_unsubscription(addr, subscription_table[addr], hops);
        if(subscription_table.has(victim_addr)) {
            submit_unsubscription(victim_addr, subscription_table[victim_addr], hops);
        }
      }
      void subscribe_address(long addr, int req_vault, int val_vault) {
        // Calculate "hops" - it is actually the number of cycles it takes for the subscribe request to finish
        int hops = calculate_hops_travelled(req_vault, val_vault, READ_LENGTH);
        // Calculate the address vector based on the address
        vector<int> addr_vec = mem_ptr -> address_to_address_vector(addr);
        // We find the "victim address vector" so we can swap that address with our desired subscription address
        // The "vidtim address" is currently found by locating the address with exactly same row and column as the desired remote address in the local vault
        vector<int> victim_vec(addr_vec);
        victim_vec[int(HMC::Level::Vault)] = req_vault; // We are locating the page in the local vault's same row & column for swapping with the remote vault
        long victim_addr = mem_ptr -> address_vector_to_address(victim_vec);
        // cout << "We swap address " << addr << " with address " << victim_addr << " it will take " << hops << " cycles to complete" << endl;
        submit_subscription(addr, req_vault, hops); // Submit to wait for given number of cycles
        submit_subscription(victim_addr, val_vault, hops);
      }
      void submit_subscription(long addr, int mapped_vault, int hops) {
        total_submitted_subscriptions++;
        // We find the "original vault" of the address as we need to update its subscription table too (in the case of multi-core subscription table)
        int original_vault = find_original_vault_of_address(addr);
        pending_subscription.push_back(SubscriptionTask(addr, original_vault, mapped_vault, hops));
      }
      void submit_unsubscription(long addr, int mapped_vault, int hops) {
        // We find the "original vault" of the address as we need to update its subscription table too (in the case of multi-core subscription table)
        int original_vault = find_original_vault_of_address(addr);
        pending_unsubscription.push_back(SubscriptionTask(addr, original_vault, mapped_vault, hops));
      }
      void immediate_unsubscribe_address(int original_vault, long addr) { // unless otherwise specifiedd, "addr" in arguments below are preprocessed addresses
        if(subscription_table.has(addr)){
          int current_vault = subscription_table[addr];
          if(subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::LRU) {
            // LRU Logic - We try to erase both the original and value vaults' entry of the address
            lru_units[current_vault].erase(addr);
            lru_units[original_vault].erase(addr);
          } else if (subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::LFU) {
            // LFU Logic - We try to erase both the original and value vaults' entry of the address
            lfu_units[current_vault].erase(addr);
            lfu_units[original_vault].erase(addr);
          } // If it's dirty LFU, we do nothing since it we want to keep the count even if it's unsubscribed
          subscription_table.immediate_unsubscribe_address(original_vault, addr);
          total_unsubscriptions++;
        }
      }
      void immediate_subscribe_address(int original_vault, int req_vault, long addr) {
        // Unsubscribe the address if it is already subscribed to another vault (in the case of re-subscription) - to make implementation simpler
        if(subscription_table.has(addr)){
          immediate_unsubscribe_address(original_vault, addr); // Unscribe first to make sure we're not having any issues
          total_unsubscriptions_as_a_result_of_replacement++;
        }
        if(subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::LRU) {
          // Update the LRU entries of the address in from table (located in the original vault) and to table (located in the current subscribed vault)
          lru_units[original_vault].touch(addr);
          lru_units[req_vault].touch(addr);
        } else if(subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::LFU || subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::DirtyLFU) {
          // Update the LFU entries of the address in from table (located in the original vault) and to table (located in the current subscribed vault)
          lfu_units[original_vault].touch(addr);
          lfu_units[req_vault].touch(addr);
        }
        // cout << "Immediately subscribing address " << addr << " from " << original_vault << " to " << req_vault << endl;
        subscription_table.immediate_subscribe_address(original_vault, req_vault, addr);
        total_successful_subscriptions++;
      }
      void tick() {
        // First, we check if there is any subscription buffer in pending (i.e. arrived but cannot be subscribed due to subscription table space constraints)
        // New algorithm, should run faster now that we only check the "empty" sets which are likely to be fewer
        if(subscription_buffer.size() > 0) {
          list<SubscriptionTask> new_subscription_buffer;
          // Iterate through all entries in the buffer - this will take long when buffer is huge so buffer size has to be small
          for(auto& i:subscription_buffer){
            // If we can subscripe it now, we do so
            if(subscription_table.can_insert_to_table(i.original_vault, i.req_vault, i.addr)){
              // cout << "Trying to insert " << i.addr << " into subscription table since we have space now." << endl;
              immediate_subscribe_address(i.original_vault, i.req_vault, i.addr);
              subscription_buffer_map.erase(i.addr);
              total_subscription_from_buffer++;
            } else { // Otherwise, we insert it back to the buffer pending the next cycle
              new_subscription_buffer.push_back(i);
              subscription_buffer_map[i.addr] = prev(new_subscription_buffer.end());
            }
          }
          subscription_buffer = new_subscription_buffer;
        }

        // Then, we process the transfer of subscription requests in the network
        list<SubscriptionTask> new_pending_subscription;
        for (auto& i : pending_subscription) {
          if(i.hops == 0){
            if(subscription_table.can_insert_to_table(i.original_vault, i.req_vault, i.addr)) {
              immediate_subscribe_address(i.original_vault, i.req_vault, i.addr);
            } else {
              total_unsuccessful_subscriptions++;
              // If the address is already in the subscription buffer, we do not put it in again
              if(subscription_buffer_map.count(i.addr)) {
                typename list<SubscriptionTask>::iterator it = subscription_buffer_map[i.addr];
                if(it -> req_vault != i.req_vault) {
                  it -> req_vault = i.req_vault; // If the subscription task is already in the buffer but to a different address, we change it
                }
              // Otherwise, we try to make some new space and insert the task into the buffer
              } else {
                // if the subscription table (either the original or destination or both) is full when the request arrives, we try to free up a subscription table entry
                // We first check if the original vault (i.e. where the vault originally was)'s subscription table is full
                // If so, we try to evict something from it
                if(!subscription_table.subscription_table_is_free(i.original_vault, i.addr)){
                  long victim_addr = find_victim_for_unsubscription(i.original_vault, i.addr);
                  // cout << "We pick " << victim_addr << " to evict from the table." << endl;
                  unsubscribe_address(victim_addr);
                }

                // Then, we check if the destination vault (i.e. where we're trying to move the data to)'s subscription table is full
                // We check and attempt to evict from both sides as the eviction may be different entries
                if(!subscription_table.subscription_table_is_free(i.req_vault, i.addr)){
                  long victim_addr = find_victim_for_unsubscription(i.req_vault, i.addr);
                  // cout << "We pick " << victim_addr << " to evict from the table." << endl;
                  unsubscribe_address(victim_addr);
                }

                // But the unsubscription won't take effect instantly, so we have to put the subscription request in a buffer and wait
                // If the buffer is even full, we do nothing further (and there is nothing we can do)
                if(subscription_buffer_is_free(i.req_vault)) {
                  // cout << "We push " << i.addr << " into the subscription buffer to be subscribed in the future, pending unsubscription of " << victim_addr << endl;
                  subscription_buffer.push_back(i);
                  subscription_buffer_map[i.addr] = prev(subscription_buffer.end());
                  // cout << "The content of the buffer is now: ";
                  // for(auto& i:subscription_buffer){cout << i.addr << " ";}
                  // cout << endl;
                  total_buffer_successful_insertation++;
                } else {
                  // cout << "We cannot push " << i.addr << " because the subscription buffer is full" << endl;
                  total_buffer_unsuccessful_insertation++;
                }

              }
            }
            continue;
          } // Safety Check

          i.hops -= 1;
          new_pending_subscription.push_back(i);
        }
        pending_subscription = new_pending_subscription;

        // Last, we process the pending unsubscription requests in the network
        list<SubscriptionTask> new_pending_unsubscription;
        for (auto& i : pending_unsubscription) {
          if(i.hops == 0){
            // cout << "Immediately unsubscribing address " << i.addr << endl;
            immediate_unsubscribe_address(i.original_vault, i.addr);
            continue;
          } // Safety Check

          i.hops -= 1;
          new_pending_unsubscription.push_back(i);
        }
        pending_unsubscription = new_pending_unsubscription;
      }
      void pre_process_addr(long& addr) {
        mem_ptr -> clear_lower_bits(addr, mem_ptr -> tx_bits + 1);
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
        if(subscription_table.has(addr)) {
          val_vault_id = subscription_table[addr];
          if(subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::LRU) {
            // Update the LRU entries of the address in from table (located in the original vault) and to table (located in the current subscribed vault)
            lru_units[original_vault_id].touch(addr);
            lru_units[val_vault_id].touch(addr);
          } else if(subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::LFU || subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::DirtyLFU) {
            // Update the LFU entries of the address in from table (located in the original vault) and to table (located in the current subscribed vault)
            lfu_units[original_vault_id].touch(addr);
            lfu_units[val_vault_id].touch(addr);
          }
          // cout << "Redirecting " << addr << " to vault " << subscription_table[addr] << " because it is subscribed" << endl;
          // Also, we set the address vector's vault to the subscribed vault so it can be sent to the correct vault for processing.
          req.addr_vec[int(HMC::Level::Vault)] = val_vault_id;
        }
        // Calculate hops and count for prefetch policy check and implementation
        uint64_t hops = (uint64_t)calculate_hops_travelled(req_vault_id, val_vault_id, OTHER_LENGTH);
        uint64_t count = count_table.update_counter_table_and_get_count(req_vault_id, addr);
        // If the policy says that we should subscribe this address, we subscribe it to the requester's vault so it is closer when accessed in the future
        if(check_prefetch(hops, count)) {
          // TODO: prevent duplicate subscription
          // cout << "Address " << addr << " with hop " << hops << " and count " << count << " meets subscription threshold. We now subscribe it from " << val_vault_id << " to " << req_vault_id << endl;
          subscribe_address(addr, req_vault_id, val_vault_id);
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
        cout << "Total Unsubscription as a result of replacing existing entry: " << total_unsubscriptions_as_a_result_of_replacement << endl;
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
        assert(address_vector_to_address(addr_vec) == addr); // Test script to make sure the implementation is correct.
        req.addr_vec = addr_vec;

        if (subscription_prefetcher_type != SubscriptionPrefetcherType::None) {
          prefetcher_set.access_address(req);
        }

        req.arrive_hmc = clk;

        if(pim_mode_enabled){
            // To model NOC traffic
            //I'm considering 32 vaults. So the 2D mesh will be 36x36
            //To calculate how many hops, check the manhattan distance
            int destination_vault = req.addr_vec[int(HMC::Level::Vault)];

            int origin_vault = req.coreid;
            int hops;
            if(!network_overhead) {
              hops = 0;
            }
            else if (req.type == Request::Type::READ){
              // Let's assume 1 Flit = 128 bytes
              // A read request is 64 bytes
              // One read request will take = 1 Flit*hops + 5*hops
              hops = calculate_hops_travelled(origin_vault, destination_vault, READ_LENGTH);
            }
            else if (req.type == Request::Type::WRITE){
              hops = calculate_hops_travelled(origin_vault, destination_vault, WRITE_LENGTH);
            } else {
              hops = calculate_hops_travelled(origin_vault, destination_vault, OTHER_LENGTH);
            }
            if(network_overhead) {
              total_hops += calculate_hops_travelled(origin_vault, destination_vault, OTHER_LENGTH);
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
