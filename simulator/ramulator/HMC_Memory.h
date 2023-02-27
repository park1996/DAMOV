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
#include <array>

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
  bool num_cores;
public:
    long clk = 0;
    bool pim_mode_enabled = false;
    bool network_overhead = false;

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

    // A subscription based prefetcher
    template <typename TableType>
    class SubscriptionPrefetcherSet {
    private:
        static const int COUNTER_TABLE_SIZE = 1024;
        static const int COUNTER_BITS = 16;
        static const int TAG_BITS = 16;
        vector<array<TableType, COUNTER_TABLE_SIZE>> count_tables;
        unordered_map<long, int> address_translation_table; // Subscribe remote address (1st val) to local address (2nd address)
        unordered_map<long, long> address_swap_table; // Subscribe remote address (1st val) to local address (2nd address)
    public:
        SubscriptionPrefetcherSet(int controllers):count_tables(controllers, array<TableType, COUNTER_TABLE_SIZE>()) {}
        int get_counter_table_size() const {return COUNTER_TABLE_SIZE;}
        bool check_prefetch(TableType hops, TableType count) {
            // TODO: Implment a good prefetch policy
            return hops >= 5 && count >= 1;
        }
        void unsubscribe_address(long address) {
            if(address_translation_table.count(address) == 0) {
                return;
            }
            // TODO: Swap the "victim address" back
            address_translation_table.erase(address);
        }
        int subscribe_address(long address, int vault) {
            // TODO: Determine a "victim address to be swapped over"
            unsubscribe_address(address); // Unscribe first to make sure we're not having any issues
            address_translation_table[address] = vault;
            return vault;
        }
        int find_vault(long address, int original_vault) {
            if(address_translation_table.count(address)) {
                return address_translation_table[address];
            }
            return original_vault;
        }
        void update_counter_table(Request req) {
            long addr = req.addr;
            int val_vault_id = find_vault(addr, req.addr_vec[int(HMC::Level::Vault)]);
            int req_vault_id = req.coreid;
            long table_index = addr / 64 % COUNTER_TABLE_SIZE; // 64 bits per flip, and we prefetch by flip
            TableType table_entry = count_tables[req_vault_id][table_index]; // Requesting core is in charge of keeping track
            TableType tag = 0;
            long temp_addr = addr;
            while (temp_addr != 0) {
                tag ^= temp_addr;
                temp_addr = temp_addr >> TAG_BITS;
            }
            tag = (tag << (COUNTER_BITS)) >> (COUNTER_BITS);
            TableType count;
            TableType old_tag = (table_entry >> (COUNTER_BITS));
            if(old_tag != tag) {
                count = 0; // If tag does not match, the address is not the same and we start from the scratch
                // cout << "A prefetch table replacement happening at index: " << table_index << " and vault " << req.addr_vec[int(HMC::Level::Vault)] <<
                //     " The old tag is " << old_tag << " the new tag is " << tag << endl;
            } else {
                // cout << "No replacement is happening as the old tag is the same as the new tag! Index: " << table_index << " vault: " << req.addr_vec[int(HMC::Level::Vault)] << " old tag: " <<
                //     old_tag << " new tag: " << tag << endl;   
                TableType count = (table_entry << TAG_BITS >>  TAG_BITS);
            }
            count++;
            if(count >= (1 << COUNTER_BITS)) {
                count = (1 << COUNTER_BITS) - 1;
            }
            int vault_destination_x = val_vault_id/6;
            int vault_destination_y = val_vault_id%6;
            int vault_origin_x = req_vault_id/6;
            int vault_origin_y = req_vault_id%6;
            TableType hops = abs(vault_destination_x - vault_origin_x) + abs(vault_destination_y - vault_origin_y);
            count_tables[req_vault_id][table_index] = (tag << (COUNTER_BITS)) | count;
            if(check_prefetch(hops, count)) {
                // cout << "[RAMULATOR] Subscribing memory from vault " << req.addr_vec[int(HMC::Level::Vault)] << " to core " << req.coreid << ". Inserted in index " << table_index << endl;
                subscribe_address(addr, req_vault_id);
            }
        }
    };

    SubscriptionPrefetcherSet<uint32_t> prefetcher_set;

    // class SubscriptionPrefetcherSet::SubscriptionPrefetcher {
    // private:
    //     static const int COUNTER_TABLE_SIZE = 1024;
    //     static const int COUNTER_BITS = 16;
    //     static const int HOP_BITS = 4;
    //     static const int TAG_BITS = 12;
    //     TableType counter_table[COUNTER_TABLE_SIZE];
    //     unordered_map<long, int> subscription_table; // Subscribe remote address (1st val) to local address (2nd address)
    // public:
    //     int get_counter_table_size() const {return COUNTER_TABLE_SIZE;}
    //     bool check_prefetch(TableType hops, TableType count) {
    //         // TODO: Implment a good prefetch policy
    //         return hops >= 5 && count >= 1;
    //     }
    //     long subscribe_address(long remote_address) {
    //         // TODO: Request the remote address to be sent over. If unsucessful, return 0
    //         // TODO: Check if we have a local address to hold the subscribed data. If unsuccessful, return 0
    //         long allocated_address = 0;
    //         subscription_table[remote_address] = allocated_address;
    //         return allocated_address;
    //     }
    //     // TODO: Call remote controller's unsubscription function when writing new data to subscribed flips
    //     void unsubscribe_address(long remote_address) {
    //         // TODO: Free the local address of the subscribed data.
    //         subscription_table.erase(remote_address);
    //     }
    //     long translate_address(long remote_address) {
    //         if(subscription_table.count(remote_address)) {
    //             return subscription_table[remote_address];
    //         }
    //         return remote_address;
    //     }
    //     void update_counter_table(Request req) {
    //         long addr = req.addr;
    //         if(subscription_table.count(addr)) {
    //             // cout << "[RAMULATOR] Address " << addr << " is already subscribed." << endl;
    //             return; // Do nothing if the address is already subscribed.
    //         }
    //         long table_index = addr / 64 % COUNTER_TABLE_SIZE; // 64 bits per flip, and we prefetch by flip
    //         TableType table_entry = counter_table[table_index];
    //         TableType tag = 0;
    //         long temp_addr = addr;
    //         while (temp_addr != 0) {
    //             tag ^= temp_addr;
    //             temp_addr = temp_addr >> TAG_BITS;
    //         }
    //         tag = (tag << (COUNTER_BITS + HOP_BITS)) >> (COUNTER_BITS + HOP_BITS);
    //         TableType count;
    //         TableType old_tag = (table_entry >> (COUNTER_BITS + HOP_BITS));
    //         if(old_tag != tag) {
    //             count = 0; // If tag does not match, the address is not the same and we start from the scratch
    //             // cout << "A prefetch table replacement happening at index: " << table_index << " and vault " << req.addr_vec[int(HMC::Level::Vault)] <<
    //             //     " The old tag is " << old_tag << " the new tag is " << tag << endl;
    //         } else {
    //             // cout << "No replacement is happening as the old tag is the same as the new tag! Index: " << table_index << " vault: " << req.addr_vec[int(HMC::Level::Vault)] << " old tag: " <<
    //             //     old_tag << " new tag: " << tag << endl;   
    //             TableType count = (table_entry << (HOP_BITS + TAG_BITS) >> (HOP_BITS + TAG_BITS));
    //         }
    //         count++;
    //         if(count >= (1 << COUNTER_BITS)) {
    //             count = (1 << COUNTER_BITS) - 1;
    //         }
    //         int vault_destination_x = req.addr_vec[int(HMC::Level::Vault)]/6;
    //         int vault_destination_y = req.addr_vec[int(HMC::Level::Vault)]%6;
    //         int vault_origin_x = req.coreid/6;
    //         int vault_origin_y = req.coreid%6;
    //         TableType hops = abs(vault_destination_x - vault_origin_x) + abs(vault_destination_y - vault_origin_y);
    //         if(hops >= (1 << HOP_BITS)) {
    //             hops = (1 << HOP_BITS) - 1;
    //         }
    //         counter_table[table_index] = (tag << (COUNTER_BITS + HOP_BITS)) | (hops << COUNTER_BITS) | count;
    //         if(check_prefetch(hops, count)) {
    //             // cout << "[RAMULATOR] Subscribing memory from vault " << req.addr_vec[int(HMC::Level::Vault)] << " to core " << req.coreid << ". Inserted in index " << table_index << endl;
    //             subscribe_address(addr);
    //         }
    //     }
    // };

    vector<int> addr_bits;
    vector<vector <int> > address_distribution;

    int tx_bits;

    Memory(const Config& configs, vector<Controller<HMC>*> ctrls)
        : ctrls(ctrls),
          spec(ctrls[0]->channel->spec),
          addr_bits(int(HMC::Level::MAX)),
          prefetcher_set(ctrls.size())
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

    bool send(Request req)
    {
      //  cout << "receive request packets@host controller with address " << req.addr << endl;
        req._addr = req.addr;
        req.addr_vec.resize(addr_bits.size());
        req.reqid = mem_req_count;


        clear_higher_bits(req.addr, max_address-1ll);
        long addr = req.addr;
        long coreid = req.coreid;

        // Each transaction size is 2^tx_bits, so first clear the lowest tx_bits bits
        clear_lower_bits(addr, tx_bits);

        switch(int(type)) {
          case int(Type::RoCoBaVa): {
            int max_block_col_bits =
                spec->maxblock_entry.flit_num_bits - tx_bits;
            req.addr_vec[int(HMC::Level::Column)] =
                slice_lower_bits(addr, max_block_col_bits);
            req.addr_vec[int(HMC::Level::Vault)] =
                slice_lower_bits(addr, addr_bits[int(HMC::Level::Vault)]);
            req.addr_vec[int(HMC::Level::Bank)] =
                slice_lower_bits(addr, addr_bits[int(HMC::Level::Bank)]);
            req.addr_vec[int(HMC::Level::BankGroup)] =
                slice_lower_bits(addr, addr_bits[int(HMC::Level::BankGroup)]);
            int column_MSB_bits =
              slice_lower_bits(
                  addr, addr_bits[int(HMC::Level::Column)] - max_block_col_bits);
            req.addr_vec[int(HMC::Level::Column)] =
              req.addr_vec[int(HMC::Level::Column)] | (column_MSB_bits << max_block_col_bits);
            req.addr_vec[int(HMC::Level::Row)] =
                slice_lower_bits(addr, addr_bits[int(HMC::Level::Row)]);
          }
          break;
          case int(Type::RoBaCoVa): {
            int max_block_col_bits =
                spec->maxblock_entry.flit_num_bits - tx_bits;
            req.addr_vec[int(HMC::Level::Column)] =
                slice_lower_bits(addr, max_block_col_bits);
            req.addr_vec[int(HMC::Level::Vault)] =
                slice_lower_bits(addr, addr_bits[int(HMC::Level::Vault)]);
            int column_MSB_bits =
              slice_lower_bits(
                  addr, addr_bits[int(HMC::Level::Column)] - max_block_col_bits);
            req.addr_vec[int(HMC::Level::Column)] =
              req.addr_vec[int(HMC::Level::Column)] | (column_MSB_bits << max_block_col_bits);
            req.addr_vec[int(HMC::Level::Bank)] =
                slice_lower_bits(addr, addr_bits[int(HMC::Level::Bank)]);
            req.addr_vec[int(HMC::Level::BankGroup)] =
                slice_lower_bits(addr, addr_bits[int(HMC::Level::BankGroup)]);
            req.addr_vec[int(HMC::Level::Row)] =
                slice_lower_bits(addr, addr_bits[int(HMC::Level::Row)]);
          }
          break;
          case int(Type::RoCoBaBgVa): {
            int max_block_col_bits =
                spec->maxblock_entry.flit_num_bits - tx_bits;
            req.addr_vec[int(HMC::Level::Column)] =
                slice_lower_bits(addr, max_block_col_bits);
            req.addr_vec[int(HMC::Level::Vault)] =
                slice_lower_bits(addr, addr_bits[int(HMC::Level::Vault)]);
            req.addr_vec[int(HMC::Level::BankGroup)] =
                slice_lower_bits(addr, addr_bits[int(HMC::Level::BankGroup)]);
            req.addr_vec[int(HMC::Level::Bank)] =
                slice_lower_bits(addr, addr_bits[int(HMC::Level::Bank)]);
            int column_MSB_bits =
              slice_lower_bits(
                  addr, addr_bits[int(HMC::Level::Column)] - max_block_col_bits);
            req.addr_vec[int(HMC::Level::Column)] =
              req.addr_vec[int(HMC::Level::Column)] | (column_MSB_bits << max_block_col_bits);
            req.addr_vec[int(HMC::Level::Row)] =
                slice_lower_bits(addr, addr_bits[int(HMC::Level::Row)]);
          }
          break;
          default:
              assert(false);
        }

        req.arrive_hmc = clk;

        if(pim_mode_enabled){
            // To model NOC traffic
            //I'm considering 32 vaults. So the 2D mesh will be 36x36
            //To calculate how many hops, check the manhattan distance
            int vault_destination_x = prefetcher_set.find_vault(req.addr, req.addr_vec[int(HMC::Level::Vault)])/6;
            int vault_destination_y = prefetcher_set.find_vault(req.addr, req.addr_vec[int(HMC::Level::Vault)])%6;

            int vault_origin_x = req.coreid/6;
            int vault_origin_y = req.coreid%6;

            int hops = abs(vault_destination_x - vault_origin_x) + abs(vault_destination_y - vault_origin_y);
            if(!network_overhead) hops = 0;
            if (req.type == Request::Type::READ){
              // Let's assume 1 Flit = 128 bytes
              // A read request is 64 bytes
              // One read request will take = 1 Flit*hops + 5*hops
              hops = hops*6;
            }
            else if (req.type == Request::Type::WRITE){
              hops = hops*5;
            }
            req.hops = hops;

            if(!ctrls[req.addr_vec[int(HMC::Level::Vault)]] -> receive(req)){
              return false;
            }
            prefetcher_set.update_counter_table(req);

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
