from __future__ import print_function
import os
import sys
import math
import struct
import sqlite3
sys.path.append(os.environ['PERF_EXEC_PATH'] + \
        '/scripts/python/Perf-Trace-Util/lib/Perf/Trace')
from perf_trace_context import *
from EventClass import *
con = sqlite3.connect("/dev/shm/perf.db")
con.isolation_level = None
def trace_begin():
        print("In trace_begin:\n")
        con.execute("""
                create table if not exists gen_events (
                        name text,
                        symbol text,
                        comm text,
                        dso text
                );""")
        con.execute("""
                create table if not exists pebs_ll (
                        name text,
                        symbol text,
                        comm text,
                        dso text,
                        flags integer,
                        ip integer,
                        status integer,
                        dse integer,
                        dla integer,
                        lat integer
                );""")
def process_event(param_dict):
        event_attr = param_dict["attr"]
        sample     = param_dict["sample"]
        raw_buf    = param_dict["raw_buf"]
        comm       = param_dict["comm"]
        name       = param_dict["ev_name"]
        if ("dso" in param_dict):
                dso = param_dict["dso"]
        else:
                dso = "Unknown_dso"
        if ("symbol" in param_dict):
                symbol = param_dict["symbol"]
        else:
                symbol = "Unknown_symbol"
        event = create_event(name, comm, dso, symbol, raw_buf)
        insert_db(event)
def insert_db(event):
        if event.ev_type == EVTYPE_GENERIC:
                con.execute("insert into gen_events values(?, ?, ?, ?)",
                                (event.name, event.symbol, event.comm, event.dso))
        elif event.ev_type == EVTYPE_PEBS_LL:
                event.ip &= 0x7fffffffffffffff
                event.dla &= 0x7fffffffffffffff
                con.execute("insert into pebs_ll values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                        (event.name, event.symbol, event.comm, event.dso, event.flags,
                                event.ip, event.status, event.dse, event.dla, event.lat))
def trace_end():
        print("In trace_end:\n")
        show_general_events()
        show_pebs_ll()
        con.close()
def num2sym(num):
        snum = '
        return snum
def show_general_events():
        count = con.execute("select count(*) from gen_events")
        for t in count:
                print("There is %d records in gen_events table" % t[0])
                if t[0] == 0:
                        return
        print("Statistics about the general events grouped by thread/symbol/dso: \n")
        commq = con.execute("select comm, count(comm) from gen_events group by comm order by -count(comm)")
        print("\n%16s %8s %16s\n%s" % ("comm", "number", "histogram", "="*42))
        for row in commq:
             print("%16s %8d     %s" % (row[0], row[1], num2sym(row[1])))
        print("\n%32s %8s %16s\n%s" % ("symbol", "number", "histogram", "="*58))
        symbolq = con.execute("select symbol, count(symbol) from gen_events group by symbol order by -count(symbol)")
        for row in symbolq:
             print("%32s %8d     %s" % (row[0], row[1], num2sym(row[1])))
        print("\n%40s %8s %16s\n%s" % ("dso", "number", "histogram", "="*74))
        dsoq = con.execute("select dso, count(dso) from gen_events group by dso order by -count(dso)")
        for row in dsoq:
             print("%40s %8d     %s" % (row[0], row[1], num2sym(row[1])))
def show_pebs_ll():
        count = con.execute("select count(*) from pebs_ll")
        for t in count:
                print("There is %d records in pebs_ll table" % t[0])
                if t[0] == 0:
                        return
        print("Statistics about the PEBS Load Latency events grouped by thread/symbol/dse/latency: \n")
        commq = con.execute("select comm, count(comm) from pebs_ll group by comm order by -count(comm)")
        print("\n%16s %8s %16s\n%s" % ("comm", "number", "histogram", "="*42))
        for row in commq:
             print("%16s %8d     %s" % (row[0], row[1], num2sym(row[1])))
        print("\n%32s %8s %16s\n%s" % ("symbol", "number", "histogram", "="*58))
        symbolq = con.execute("select symbol, count(symbol) from pebs_ll group by symbol order by -count(symbol)")
        for row in symbolq:
             print("%32s %8d     %s" % (row[0], row[1], num2sym(row[1])))
        dseq = con.execute("select dse, count(dse) from pebs_ll group by dse order by -count(dse)")
        print("\n%32s %8s %16s\n%s" % ("dse", "number", "histogram", "="*58))
        for row in dseq:
             print("%32s %8d     %s" % (row[0], row[1], num2sym(row[1])))
        latq = con.execute("select lat, count(lat) from pebs_ll group by lat order by lat")
        print("\n%32s %8s %16s\n%s" % ("latency", "number", "histogram", "="*58))
        for row in latq:
             print("%32s %8d     %s" % (row[0], row[1], num2sym(row[1])))
def trace_unhandled(event_name, context, event_fields_dict):
        print (' '.join(['%s=%s'%(k,str(v))for k,v in sorted(event_fields_dict.items())]))
