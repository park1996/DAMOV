#ifndef __HMC_CONTROLLER_H
#define __HMC_CONTROLLER_H

#include <cassert>
#include <cstdio>
#include <deque>
#include <fstream>
#include <iostream>
#include <list>
#include <string>
#include <vector>
#include <unordered_map>
#include "Controller.h"
#include "Scheduler.h"

#include "HMC.h"
#include "Packet.h"

//#include "libdrampower/LibDRAMPower.h"
//#include "xmlparser/MemSpecParser.h"

using namespace std;

namespace ramulator
{

template <>
class Controller<HMC>
{
public:
    // For counting bandwidth
    ScalarStat* read_transaction_bytes;
    ScalarStat* write_transaction_bytes;

    ScalarStat* row_hits;
    ScalarStat* row_misses;
    ScalarStat* row_conflicts;
    VectorStat* read_row_hits;
    VectorStat* read_row_misses;
    VectorStat* read_row_conflicts;
    VectorStat* write_row_hits;
    VectorStat* write_row_misses;
    VectorStat* write_row_conflicts;

    ScalarStat* queueing_latency_sum;

    ScalarStat* req_queue_length_sum;
    ScalarStat* read_req_queue_length_sum;
    ScalarStat* write_req_queue_length_sum;

    VectorStat* record_read_hits;
    VectorStat* record_read_misses;
    VectorStat* record_read_conflicts;
    VectorStat* record_write_hits;
    VectorStat* record_write_misses;
    VectorStat* record_write_conflicts;
    // DRAM power estimation statistics

    ScalarStat act_energy;
    ScalarStat pre_energy;
    ScalarStat read_energy;
    ScalarStat write_energy;

    ScalarStat act_stdby_energy;
    ScalarStat pre_stdby_energy;
    ScalarStat idle_energy_act;
    ScalarStat idle_energy_pre;

    ScalarStat f_act_pd_energy;
    ScalarStat f_pre_pd_energy;
    ScalarStat s_act_pd_energy;
    ScalarStat s_pre_pd_energy;
    ScalarStat sref_energy;
    ScalarStat sref_ref_energy;
    ScalarStat sref_ref_act_energy;
    ScalarStat sref_ref_pre_energy;

    ScalarStat spup_energy;
    ScalarStat spup_ref_energy;
    ScalarStat spup_ref_act_energy;
    ScalarStat spup_ref_pre_energy;
    ScalarStat pup_act_energy;
    ScalarStat pup_pre_energy;

    ScalarStat IO_power;
    ScalarStat WR_ODT_power;
    ScalarStat TermRD_power;
    ScalarStat TermWR_power;

    ScalarStat read_io_energy;
    ScalarStat write_term_energy;
    ScalarStat read_oterm_energy;
    ScalarStat write_oterm_energy;
    ScalarStat io_term_energy;

    ScalarStat ref_energy;

    ScalarStat total_energy;
    ScalarStat average_power;

    // drampower counter

    // Number of activate commands
    ScalarStat numberofacts_s;
    // Number of precharge commands
    ScalarStat numberofpres_s;
    // Number of reads commands
    ScalarStat numberofreads_s;
    // Number of writes commands
    ScalarStat numberofwrites_s;
    // Number of refresh commands
    ScalarStat numberofrefs_s;
    // Number of precharge cycles
    ScalarStat precycles_s;
    // Number of active cycles
    ScalarStat actcycles_s;
    // Number of Idle cycles in the active state
    ScalarStat idlecycles_act_s;
    // Number of Idle cycles in the precharge state
    ScalarStat idlecycles_pre_s;
    // Number of fast-exit activate power-downs
    ScalarStat f_act_pdns_s;
    // Number of slow-exit activate power-downs
    ScalarStat s_act_pdns_s;
    // Number of fast-exit precharged power-downs
    ScalarStat f_pre_pdns_s;
    // Number of slow-exit activate power-downs
    ScalarStat s_pre_pdns_s;
    // Number of self-refresh commands
    ScalarStat numberofsrefs_s;
    // Number of clock cycles in fast-exit activate power-down mode
    ScalarStat f_act_pdcycles_s;
    // Number of clock cycles in slow-exit activate power-down mode
    ScalarStat s_act_pdcycles_s;
    // Number of clock cycles in fast-exit precharged power-down mode
    ScalarStat f_pre_pdcycles_s;
    // Number of clock cycles in slow-exit precharged power-down mode
    ScalarStat s_pre_pdcycles_s;
    // Number of clock cycles in self-refresh mode
    ScalarStat sref_cycles_s;
    // Number of clock cycles in activate power-up mode
    ScalarStat pup_act_cycles_s;
    // Number of clock cycles in precharged power-up mode
    ScalarStat pup_pre_cycles_s;
    // Number of clock cycles in self-refresh power-up mode
    ScalarStat spup_cycles_s;

    // Number of active auto-refresh cycles in self-refresh mode
    ScalarStat sref_ref_act_cycles_s;
    // Number of precharged auto-refresh cycles in self-refresh mode
    ScalarStat sref_ref_pre_cycles_s;
    // Number of active auto-refresh cycles during self-refresh exit
    ScalarStat spup_ref_act_cycles_s;
    // Number of precharged auto-refresh cycles during self-refresh exit
    ScalarStat spup_ref_pre_cycles_s;

    //libDRAMPower* drampower;
    long update_counter = 0;

public:
    /* Member Variables */
    long clk = 0;
    DRAM<HMC>* channel;

    Scheduler<HMC>* scheduler;  // determines the highest priority request whose commands will be issued
    RowPolicy<HMC>* rowpolicy;  // determines the row-policy (e.g., closed-row vs. open-row)
    RowTable<HMC>* rowtable;  // tracks metadata about rows (e.g., which are open and for how long)
    Refresh<HMC>* refresh;
    long total_hmc_latency = 0;
    long total_latency = 0;
    long total_cycle_waiting_not_ready_request = 0;

    struct Queue {
        list<Request> q;
        list<Request> arrivel_q;
        size_t max_q_size = 0;
        size_t max_arrivel_size = 0;
        long total_pending_task = 0;
        unsigned int max = 32; // TODO queue qize
        void set_max(int max) {
          this -> max = max;
          cout << "Queue size is: " << this -> max << endl;
        }
        unsigned int size() {return arrivel_q.size() + q.size();}
        void update(){
          list<Request> tmp;
          for (auto& i : arrivel_q) {
            assert(i.hops <= MAX_HOP);
            if(i.hops == 0){
              total_pending_task+=q.size();
              q.push_back(i);
              continue;
            }
            i.hops -= 1;
            tmp.push_back(i);
          }
          arrivel_q = tmp;
          if(q.size() > max_q_size) {
            max_q_size = q.size();
          }
          if(arrivel_q.size() > max_arrivel_size) {
            max_arrivel_size = arrivel_q.size();
          }
        }
        void arrive(Request& req) {
            if(req.hops == 0) {
                q.push_back(req);
            } else {
                arrivel_q.push_back(req);
            }
        }
    };

    Queue readq;  // queue for read requests
    Queue writeq;  // queue for write requests
    Queue otherq;  // queue for all "other" requests (e.g., refresh)
    Queue overflow;

    struct PendingQueue {
        deque<Request> q;
        deque<Request> arrivel_q;
        unsigned int size() {return q.size()+arrivel_q.size();}
        void update(){
          deque<Request> tmp;
          for (auto& i : arrivel_q) {
            assert(i.hops <= MAX_HOP);
            if(i.hops == 0){
              q.push_back(i);
              continue;
            }
            i.hops -= 1;
            tmp.push_back(i);
          }
          arrivel_q = tmp;
        }
        void arrive(Request& req) {
            assert(req.hops <= MAX_HOP);
            if(req.hops == 0) {
                q.push_back(req);
            } else {
                arrivel_q.push_back(req);
            }
        }
        void push_back(Request& req){
            if(req.hops == 0) {
                q.push_back(req);
            } else {
                arrivel_q.push_back(req);
            }
        }
        void pop_front(){q.pop_front();}
    };

    deque<Request> pending;  // read requests that are about to receive data from DRAM
    deque<Request> pending_write;  //write requests that are about to receive data from DRAM

    bool write_mode = false;  // whether write requests should be prioritized over reads
    //long refreshed = 0;  // last time refresh requests were generated

    /* Command trace for DRAMPower 3.1 */
    string cmd_trace_prefix = "cmd-trace-";
    ofstream cmd_trace_file;
    bool record_cmd_trace = false;
    /* Commands to stdout */
    bool print_cmd_trace = false;
    bool with_drampower = false;

    // ideal DRAM
    bool no_DRAM_latency = false;
    bool unlimit_bandwidth = false;

    // HMC
    deque<Packet> response_packets_buffer;
    map<long, Packet> incoming_packets_buffer;
    bool pim_mode_enabled = false;

    /* Constructor */
    Controller(const Config& configs, DRAM<HMC>* channel) :
        channel(channel),
        scheduler(new Scheduler<HMC>(this)),
        rowpolicy(new RowPolicy<HMC>(this)),
        rowtable(new RowTable<HMC>(this)),
        refresh(new Refresh<HMC>(this))
    {
        record_cmd_trace = configs.record_cmd_trace();
        print_cmd_trace = configs.print_cmd_trace();
        if (record_cmd_trace){
            if (configs["cmd_trace_prefix"] != "") {
              cmd_trace_prefix = configs["cmd_trace_prefix"];
            }
            cmd_trace_file.open(
                cmd_trace_prefix + "chan-" + to_string(channel->id)
                + ".cmdtrace");
        }
        //if (configs["drampower_memspecs"] != "") {
          with_drampower = false;
          //drampower = new libDRAMPower(
            //  Data::MemSpecParser::getMemSpecFromXML(
              //    configs["drampower_memspecs"]),
             // true);
      //  }
        if (configs["no_DRAM_latency"] == "true") {
          no_DRAM_latency = true;
          scheduler->type = Scheduler<HMC>::Type::FRFCFS;
        }
        if (configs["unlimit_bandwidth"] == "true") {
          unlimit_bandwidth = true;
          printf("nBL: %d\n", channel->spec->speed_entry.nBL);
          assert(channel->spec->speed_entry.nBL == 0);
          assert(channel->spec->read_latency == channel->spec->speed_entry.nCL);
          assert(channel->spec->speed_entry.nCCDS == 1);
          assert(channel->spec->speed_entry.nCCDL == 1);
        }

        if (configs.contains("hmc_queue_size")) {
          readq.set_max(stoi(configs["hmc_queue_size"]));
          writeq.set_max(stoi(configs["hmc_queue_size"]));
          otherq.set_max(stoi(configs["hmc_queue_size"]));
          overflow.set_max(stoi(configs["hmc_queue_size"]));
        }

        pim_mode_enabled = configs.pim_mode_enabled();
        if (with_drampower) {
          // init DRAMPower stats
          act_energy
              .name("act_energy_" + to_string(channel->id))
              .desc("act_energy_" + to_string(channel->id))
              .precision(6)
              ;
          pre_energy
              .name("pre_energy_" + to_string(channel->id))
              .desc("pre_energy_" + to_string(channel->id))
              .precision(6)
              ;
          read_energy
              .name("read_energy_" + to_string(channel->id))
              .desc("read_energy_" + to_string(channel->id))
              .precision(6)
              ;
          write_energy
              .name("write_energy_" + to_string(channel->id))
              .desc("write_energy_" + to_string(channel->id))
              .precision(6)
              ;

          act_stdby_energy
              .name("act_stdby_energy_" + to_string(channel->id))
              .desc("act_stdby_energy_" + to_string(channel->id))
              .precision(6)
              ;

          pre_stdby_energy
              .name("pre_stdby_energy_" + to_string(channel->id))
              .desc("pre_stdby_energy_" + to_string(channel->id))
              .precision(6)
              ;

          idle_energy_act
              .name("idle_energy_act_" + to_string(channel->id))
              .desc("idle_energy_act_" + to_string(channel->id))
              .precision(6)
              ;

          idle_energy_pre
              .name("idle_energy_pre_" + to_string(channel->id))
              .desc("idle_energy_pre_" + to_string(channel->id))
              .precision(6)
              ;

          f_act_pd_energy
              .name("f_act_pd_energy_" + to_string(channel->id))
              .desc("f_act_pd_energy_" + to_string(channel->id))
              .precision(6)
              ;
          f_pre_pd_energy
              .name("f_pre_pd_energy_" + to_string(channel->id))
              .desc("f_pre_pd_energy_" + to_string(channel->id))
              .precision(6)
              ;
          s_act_pd_energy
              .name("s_act_pd_energy_" + to_string(channel->id))
              .desc("s_act_pd_energy_" + to_string(channel->id))
              .precision(6)
              ;
          s_pre_pd_energy
              .name("s_pre_pd_energy_" + to_string(channel->id))
              .desc("s_pre_pd_energy_" + to_string(channel->id))
              .precision(6)
              ;
          sref_energy
              .name("sref_energy_" + to_string(channel->id))
              .desc("sref_energy_" + to_string(channel->id))
              .precision(6)
              ;
          sref_ref_energy
              .name("sref_ref_energy_" + to_string(channel->id))
              .desc("sref_ref_energy_" + to_string(channel->id))
              .precision(6)
              ;
          sref_ref_act_energy
              .name("sref_ref_act_energy_" + to_string(channel->id))
              .desc("sref_ref_act_energy_" + to_string(channel->id))
              .precision(6)
              ;
          sref_ref_pre_energy
              .name("sref_ref_pre_energy_" + to_string(channel->id))
              .desc("sref_ref_pre_energy_" + to_string(channel->id))
              .precision(6)
              ;

          spup_energy
              .name("spup_energy_" + to_string(channel->id))
              .desc("spup_energy_" + to_string(channel->id))
              .precision(6)
              ;
          spup_ref_energy
              .name("spup_ref_energy_" + to_string(channel->id))
              .desc("spup_ref_energy_" + to_string(channel->id))
              .precision(6)
              ;
          spup_ref_act_energy
              .name("spup_ref_act_energy_" + to_string(channel->id))
              .desc("spup_ref_act_energy_" + to_string(channel->id))
              .precision(6)
              ;
          spup_ref_pre_energy
              .name("spup_ref_pre_energy_" + to_string(channel->id))
              .desc("spup_ref_pre_energy_" + to_string(channel->id))
              .precision(6)
              ;
          pup_act_energy
              .name("pup_act_energy_" + to_string(channel->id))
              .desc("pup_act_energy_" + to_string(channel->id))
              .precision(6)
              ;
          pup_pre_energy
              .name("pup_pre_energy_" + to_string(channel->id))
              .desc("pup_pre_energy_" + to_string(channel->id))
              .precision(6)
              ;

          IO_power
              .name("IO_power_" + to_string(channel->id))
              .desc("IO_power_" + to_string(channel->id))
              .precision(6)
              ;
          WR_ODT_power
              .name("WR_ODT_power_" + to_string(channel->id))
              .desc("WR_ODT_power_" + to_string(channel->id))
              .precision(6)
              ;
          TermRD_power
              .name("TermRD_power_" + to_string(channel->id))
              .desc("TermRD_power_" + to_string(channel->id))
              .precision(6)
              ;
          TermWR_power
              .name("TermWR_power_" + to_string(channel->id))
              .desc("TermWR_power_" + to_string(channel->id))
              .precision(6)
              ;

          read_io_energy
              .name("read_io_energy_" + to_string(channel->id))
              .desc("read_io_energy_" + to_string(channel->id))
              .precision(6)
              ;
          write_term_energy
              .name("write_term_energy_" + to_string(channel->id))
              .desc("write_term_energy_" + to_string(channel->id))
              .precision(6)
              ;
          read_oterm_energy
              .name("read_oterm_energy_" + to_string(channel->id))
              .desc("read_oterm_energy_" + to_string(channel->id))
              .precision(6)
              ;
          write_oterm_energy
              .name("write_oterm_energy_" + to_string(channel->id))
              .desc("write_oterm_energy_" + to_string(channel->id))
              .precision(6)
              ;
          io_term_energy
              .name("io_term_energy_" + to_string(channel->id))
              .desc("io_term_energy_" + to_string(channel->id))
              .precision(6)
              ;

          ref_energy
              .name("ref_energy_" + to_string(channel->id))
              .desc("ref_energy_" + to_string(channel->id))
              .precision(6)
              ;

          total_energy
              .name("total_energy_" + to_string(channel->id))
              .desc("total_energy_" + to_string(channel->id))
              .precision(6)
              ;
          average_power
              .name("average_power_" + to_string(channel->id))
              .desc("average_power_" + to_string(channel->id))
              .precision(6)
              ;

          numberofacts_s
              .name("numberofacts_s_" + to_string(channel->id))
              .desc("Number of activate commands_" + to_string(channel->id))
              .precision(0)
              ;
          numberofpres_s
              .name("numberofpres_s_" + to_string(channel->id))
              .desc("Number of precharge commands_" + to_string(channel->id))
              .precision(0)
              ;
          numberofreads_s
              .name("numberofreads_s_" + to_string(channel->id))
              .desc("Number of reads commands_" + to_string(channel->id))
              .precision(0)
              ;
          numberofwrites_s
              .name("numberofwrites_s_" + to_string(channel->id))
              .desc("Number of writes commands_" + to_string(channel->id))
              .precision(0)
              ;
          numberofrefs_s
              .name("numberofrefs_s_" + to_string(channel->id))
              .desc("Number of refresh commands_" + to_string(channel->id))
              .precision(0)
              ;
          precycles_s
              .name("precycles_s_" + to_string(channel->id))
              .desc("Number of precharge cycles_" + to_string(channel->id))
              .precision(0)
              ;
          actcycles_s
              .name("actcycles_s_" + to_string(channel->id))
              .desc("Number of active cycles_" + to_string(channel->id))
              .precision(0)
              ;
          idlecycles_act_s
              .name("idlecycles_act_s_" + to_string(channel->id))
              .desc("Number of Idle cycles in the active state_" + to_string(channel->id))
              .precision(0)
              ;
          idlecycles_pre_s
              .name("idlecycles_pre_s_" + to_string(channel->id))
              .desc("Number of Idle cycles in the precharge state_" + to_string(channel->id))
              .precision(0)
              ;
          f_act_pdns_s
              .name("f_act_pdns_s_" + to_string(channel->id))
              .desc("Number of fast-exit activate power-downs_" + to_string(channel->id))
              .precision(0)
              ;
          s_act_pdns_s
              .name("s_act_pdns_s_" + to_string(channel->id))
              .desc("Number of slow-exit activate power-downs_" + to_string(channel->id))
              .precision(0)
              ;
          f_pre_pdns_s
              .name("f_pre_pdns_s_" + to_string(channel->id))
              .desc("Number of fast-exit precharged power-downs_" + to_string(channel->id))
              .precision(0)
              ;
          s_pre_pdns_s
              .name("s_pre_pdns_s_" + to_string(channel->id))
              .desc("Number of slow-exit activate power-downs_" + to_string(channel->id))
              .precision(0)
              ;
          numberofsrefs_s
              .name("numberofsrefs_s_" + to_string(channel->id))
              .desc("Number of self-refresh commands_" + to_string(channel->id))
              .precision(0)
              ;
          f_act_pdcycles_s
              .name("f_act_pdcycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in fast-exit activate power-down mode_" + to_string(channel->id))
              .precision(0)
              ;
          s_act_pdcycles_s
              .name("s_act_pdcycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in slow-exit activate power-down mode_" + to_string(channel->id))
              .precision(0)
              ;
          f_pre_pdcycles_s
              .name("f_pre_pdcycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in fast-exit precharged power-down mode_" + to_string(channel->id))
              .precision(0)
              ;
          s_pre_pdcycles_s
              .name("s_pre_pdcycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in slow-exit precharged power-down mode_" + to_string(channel->id))
              .precision(0)
              ;
          sref_cycles_s
              .name("sref_cycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in self-refresh mode_" + to_string(channel->id))
              .precision(0)
              ;
          pup_act_cycles_s
              .name("pup_act_cycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in activate power-up mode_" + to_string(channel->id))
              .precision(0)
              ;
          pup_pre_cycles_s
              .name("pup_pre_cycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in precharged power-up mode_" + to_string(channel->id))
              .precision(0)
              ;
          spup_cycles_s
              .name("spup_cycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in self-refresh power-up mode_" + to_string(channel->id))
              .precision(0)
              ;
          sref_ref_act_cycles_s
              .name("sref_ref_act_cycles_s_" + to_string(channel->id))
              .desc("Number of active auto-refresh cycles in self-refresh mode_" + to_string(channel->id))
              .precision(0)
              ;
          sref_ref_pre_cycles_s
              .name("sref_ref_pre_cycles_s_" + to_string(channel->id))
              .desc("Number of precharged auto-refresh cycles in self-refresh mode_" + to_string(channel->id))
              .precision(0)
              ;
          spup_ref_act_cycles_s
              .name("spup_ref_act_cycles_s_" + to_string(channel->id))
              .desc("Number of active auto-refresh cycles during self-refresh exit_" + to_string(channel->id))
              .precision(0)
              ;
          spup_ref_pre_cycles_s
              .name("spup_ref_pre_cycles_s_" + to_string(channel->id))
              .desc("Number of precharged auto-refresh cycles during self-refresh exit_" + to_string(channel->id))
              .precision(0)
              ;
        }
    }

    ~Controller(){
        delete scheduler;
        delete rowpolicy;
        delete rowtable;
        delete channel;
        delete refresh;
        cmd_trace_file.close();
    }

    bool receive (Packet& packet) {
      assert(packet.type == Packet::Type::REQUEST);
      Request& req = packet.req;

      if(!pim_mode_enabled)
        req.burst_count = channel->spec->burst_count;
      else
        req.burst_count = 2; //TSV = 32 bytes, request = 64 bytes -> 2 bursts

      req.transaction_bytes = channel->spec->payload_flits * 16;
      //printf("req.burst_count %d", req.burst_count);
      debug_hmc("req.reqid %d, req.coreid %d", req.reqid, req.coreid);
      // buffer packet, for future response packet
      incoming_packets_buffer[req.reqid] = packet;
      //cout << "HMC Controller received an request with address " << req.addr << endl;
      return enqueue(req);
    }

    bool receive (Request& req) {
      req.burst_count = 2; //TSV = 32 bytes, request = 64 bytes -> 2 bursts

      req.transaction_bytes = channel->spec->payload_flits * 16;
      //printf("req.burst_count %d", req.burst_count);
      debug_hmc("req.reqid %d, req.coreid %d", req.reqid, req.coreid);
      // buffer packet, for future response packet
      //cout << "HMC Controller received an request with address " << req.addr << endl;
      return enqueue(req);
    }

    void finish(long dram_cycles) {
      // finalize DRAMPower

      // finalize DRAM status
      channel->finish(dram_cycles);
    }

    /* Member Functions */
    Queue& get_queue(Request::Type type)
    {
        switch (int(type)) {
            case int(Request::Type::READ): return readq;
            case int(Request::Type::WRITE): return writeq;
            default: return otherq;
        }
    }

    bool enqueue(Request& req)
    {
        Queue& queue = get_queue(req.type);

        if (queue.max == queue.size()){
             return false;
        }

        req.arrive = clk;

        // shortcut for read requests, if a write to same addr exists
        // necessary for coherence
        if (req.type == Request::Type::READ && find_if(writeq.q.begin(), writeq.q.end(),
                [req](Request& wreq){ return req.addr == wreq.addr && req.coreid == wreq.coreid;}) != writeq.q.end()){
            req.depart = clk + 1;
            pending.push_back(req);
            req.served_without_hops = 1;
        } else {
            queue.arrive(req);
        }

        return true;
    }

    Packet form_response_packet(Request& req) {
      // All packets sent from host controller are Request packets
      assert(incoming_packets_buffer.find(req.reqid) !=
          incoming_packets_buffer.end());
      Packet req_packet = incoming_packets_buffer[req.reqid];
      int cub = req_packet.header.CUB.value;
      int tag = req_packet.header.TAG.value;
      int slid = req_packet.tail.SLID.value;
      int lng = req.type == Request::Type::WRITE ?
                1 : 1 + channel->spec->payload_flits;
      Packet::Command cmd = req_packet.header.CMD.value;
      Packet packet(Packet::Type::RESPONSE, cub, tag, lng, slid, cmd);
      packet.req = req;
      debug_hmc("cub: %d", cub);
      debug_hmc("slid: %d", slid);
      debug_hmc("lng: %d", lng);
      debug_hmc("cmd: %d", int(cmd));
      // DEBUG:
      assert(packet.header.CUB.valid());
      assert(packet.header.TAG.valid()); // -1 also considered valid here...
      assert(packet.header.SLID.valid());
      assert(packet.header.CMD.valid());
      // Don't forget to release the space for incoming packet
      incoming_packets_buffer.erase(req.reqid);

      //cout << "HMC Controller has prepared a response packet with addr " << req.addr << " and _addr " << req._addr << endl;
      return packet;
    }

    void tick()
    {
        // FIXME back to back command (add back-to-back buffer)
        clk++;
        (*req_queue_length_sum) += readq.size() + writeq.size() + pending.size();
        (*read_req_queue_length_sum) += readq.size() + pending.size();
        (*write_req_queue_length_sum) += writeq.size();


        readq.update();
        writeq.update();
        otherq.update();
        /*** 1. Serve completed reads ***/
        if (pending.size()) {
          Request& req = pending[0];
          if (req.depart <= clk) {
            if (req.depart - req.arrive > 1) {
              channel->update_serving_requests(req.addr_vec.data(), -1, clk);
            }

            if(pim_mode_enabled){
                req.depart_hmc = clk;
                total_hmc_latency += (req.depart_hmc - req.arrive_hmc);
                total_latency += (req.depart - req.arrive);
                if (req.type == Request::Type::READ || req.type == Request::Type::WRITE) {
                  req.callback(req);
                  pending.pop_front();
               }
            }
            else{
                Packet packet = form_response_packet(req);
                response_packets_buffer.push_back(packet);
                pending.pop_front();
            }
          }
        }


        /*** 2. Refresh scheduler ***/
        refresh->tick_ref();

        /*** 3. Should we schedule writes? ***/
        if (!write_mode) {
            // yes -- write queue is almost full or read queue is empty
            if ((writeq.size() >= int(0.8 * writeq.max) && writeq.q.size() > 0) || readq.size() == 0){
                write_mode = true;
            } 
        }
        else {
            // no -- write queue is almost empty and read queue is not empty
            if ((writeq.size() <= int(0.2 * writeq.max) || writeq.q.size() == 0) && readq.size() != 0) {
                write_mode = false;
            }

        }

        /*** 4. Find the best command to schedule, if any ***/
        Queue* queue = !write_mode ? &readq : &writeq;
        if (otherq.q.size())
            queue = &otherq;  // "other" requests are rare, so we give them precedence over reads/writes

        auto req = scheduler->get_head(queue->q);
        if (req == queue->q.end() || !is_ready(req)) {
            if(req != queue->q.end()) {
                if(!is_ready(req)) {
                    total_cycle_waiting_not_ready_request++;
                }
            }
          if (!no_DRAM_latency) {
            // we couldn't find a command to schedule -- let's try to be speculative
            auto cmd = HMC::Command::PRE;
            vector<int> victim = rowpolicy->get_victim(cmd);
            if (!victim.empty()){
                issue_cmd(cmd, victim);
            }
            return;  // nothing more to be done this cycle
          } else {
            return;
          }
        }
        if (req->is_first_command) {
          req->is_first_command = false;
          int coreid = req->coreid;
          if (req->type == Request::Type::READ || req->type == Request::Type::WRITE) {
            channel->update_serving_requests(req->addr_vec.data(), 1, clk);
          }
          if (req->type == Request::Type::READ) {
            (*queueing_latency_sum) += clk - req->arrive;
            if (is_row_hit(req)) {
                ++(*read_row_hits)[coreid];
                ++(*row_hits);
                debug_hmc("row hit");
            } else if (is_row_open(req)) {
                ++(*read_row_conflicts)[coreid];
                ++(*row_conflicts);
                debug_hmc("row conlict");
            } else {
                ++(*read_row_misses)[coreid];
                ++(*row_misses);
                debug_hmc("row miss");
            }
            (*read_transaction_bytes) += req->transaction_bytes;
          } else if (req->type == Request::Type::WRITE) {
            if (is_row_hit(req)) {
                ++(*write_row_hits)[coreid];
                ++(*row_hits);
            } else if (is_row_open(req)) {
                ++(*write_row_conflicts)[coreid];
                ++(*row_conflicts);
            } else {
                ++(*write_row_misses)[coreid];
                ++(*row_misses);
            }
            (*write_transaction_bytes) += req->transaction_bytes;
          }
        }

        // issue command on behalf of request
        auto cmd = get_first_cmd(req);
        issue_cmd(cmd, get_addr_vec(cmd, req));

        // check whether this is the last command (which finishes the request)
        if (cmd != channel->spec->translate[int(req->type)]){
            return;
        }

        // set a future completion time for read requests
        if (req->type == Request::Type::READ) {
            --req->burst_count;
            if (req->burst_count == 0) {
              req->depart = clk + channel->spec->read_latency;
              debug_hmc("req->depart: %ld\n", req->depart);
              pending.push_back(*req);
            }
        } else if (req->type == Request::Type::WRITE) {
            --req->burst_count;

            if (req->burst_count == 0) {
              req->depart = clk + channel->spec->write_latency;
              pending.push_back(*req);
              /*if(pim_mode_enabled){
                pending.push_back(*req);
              }
              else{
                Packet packet = form_response_packet(*req);
                response_packets_buffer.push_back(packet);
                channel->update_serving_requests(req->addr_vec.data(), -1, clk);
              }*/
            }
        }


        // remove request from queue
        if (req->burst_count == 0) {
          queue->q.erase(req);
          

          /*if (queue->size() <= queue->max){
            if(overflow.size() > 0 && overflow.q.front().type == req->type){
              queue->q.push_back(overflow.q.front());
              overflow.q.pop_front();
            }
          }*/
        }

    }

    bool is_ready(list<Request>::iterator req)
    {
        typename HMC::Command cmd = get_first_cmd(req);
        return channel->check(cmd, req->addr_vec.data(), clk);
    }

    bool is_ready(typename HMC::Command cmd, const vector<int>& addr_vec)
    {
        return channel->check(cmd, addr_vec.data(), clk);
    }

    bool is_row_hit(list<Request>::iterator req)
    {
        // cmd must be decided by the request type, not the first cmd
        typename HMC::Command cmd = channel->spec->translate[int(req->type)];
        return channel->check_row_hit(cmd, req->addr_vec.data());
    }

    bool is_row_hit(typename HMC::Command cmd, const vector<int>& addr_vec)
    {
        return channel->check_row_hit(cmd, addr_vec.data());
    }

    bool is_row_open(list<Request>::iterator req)
    {
        // cmd must be decided by the request type, not the first cmd
        typename HMC::Command cmd = channel->spec->translate[int(req->type)];
        return channel->check_row_open(cmd, req->addr_vec.data());
    }

    bool is_row_open(typename HMC::Command cmd, const vector<int>& addr_vec)
    {
        return channel->check_row_open(cmd, addr_vec.data());
    }

    void update_temp(ALDRAM::Temp current_temperature)
    {
    }

    // For telling whether this channel is busying in processing read or write
    bool is_active() {
      return (channel->cur_serving_requests > 0);
    }

    // For telling whether this channel is under refresh
    bool is_refresh() {
      return clk <= channel->end_of_refreshing;
    }

    void record_core(int coreid) {
      (*record_read_hits)[coreid] = (*read_row_hits)[coreid];
      (*record_read_misses)[coreid] = (*read_row_misses)[coreid];
      (*record_read_conflicts)[coreid] = (*read_row_conflicts)[coreid];
      (*record_write_hits)[coreid] = (*write_row_hits)[coreid];
      (*record_write_misses)[coreid] = (*write_row_misses)[coreid];
      (*record_write_conflicts)[coreid] = (*write_row_conflicts)[coreid];
    }

private:
    typename HMC::Command get_first_cmd(list<Request>::iterator req)
    {
        typename HMC::Command cmd = channel->spec->translate[int(req->type)];
        if (!no_DRAM_latency) {
          return channel->decode(cmd, req->addr_vec.data());
        } else {
          return cmd;
        }
    }

    void issue_cmd(typename HMC::Command cmd, const vector<int>& addr_vec)
    {
        // update power estimation
        if (with_drampower) {

          const string& cmd_name = channel->spec->command_name[int(cmd)];
          int bank_id = addr_vec[int(HMC::Level::Bank)];
          bank_id += addr_vec[int(HMC::Level::Bank) - 1] * channel->spec->org_entry.count[int(HMC::Level::Bank)];
          printf("%ld %d\n", clk, bank_id);
          //drampower->doCommand(Data::MemCommand::getTypeFromName(cmd_name), bank_id, clk);

          update_counter++;
          if (update_counter == 1000000) {
            //  drampower->updateCounters(false); // not the last update
              update_counter = 0;
          }
        }

        if (print_cmd_trace){
            printf("%5s %10ld:", channel->spec->command_name[int(cmd)].c_str(), clk);
            for (int lev = 0; lev < int(HMC::Level::MAX); lev++)
                printf(" %5d", addr_vec[lev]);
            printf("\n");
        }
        assert(is_ready(cmd, addr_vec));
        if (!no_DRAM_latency) {
          channel->update(cmd, addr_vec.data(), clk);
          rowtable->update(cmd, addr_vec, clk);
        } else {
          // still have bandwidth restriction (update timing for RD/WR requets)
          channel->update_timing(cmd, addr_vec.data(), clk);
        }
        if (record_cmd_trace){
            // select rank
            string& cmd_name = channel->spec->command_name[int(cmd)];
            cmd_trace_file<<clk<<','<<cmd_name;
            // TODO bad coding here
            if (cmd_name == "PREA" || cmd_name == "REF")
                cmd_trace_file<<endl;
            else{
                int bank_id = addr_vec[int(HMC::Level::Bank)];
                bank_id += addr_vec[int(HMC::Level::Bank) - 1] * channel->spec->org_entry.count[int(HMC::Level::Bank)];
                cmd_trace_file<<','<<bank_id<<endl;
            }
        }
    }
    vector<int> get_addr_vec(typename HMC::Command cmd, list<Request>::iterator req){
        return req->addr_vec;
    }
};

} /*namespace ramulator*/

#endif /*__HMC_CONTROLLER_H*/
