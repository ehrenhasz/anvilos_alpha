import os
import sys
import time
import json
import string
import random
import argparse
import threading
import webbrowser
import urllib.parse
from os import system
from functools import reduce
from dataclasses import dataclass, field
from http.server import HTTPServer, SimpleHTTPRequestHandler, test
from typing import List, Dict, Optional, NamedTuple, Set, Tuple, Any
sys.path.append(os.environ['PERF_EXEC_PATH'] + \
	'/scripts/python/Perf-Trace-Util/lib/Perf/Trace')
from perf_trace_context import *
from Core import *
StringID = int
StackID = int
FrameID = int
CategoryID = int
Milliseconds = float
start_time = None
CATEGORIES = None
PRODUCT = os.popen('uname -op').read().strip()
output_file = None
tid_to_thread = dict()
http_server_thread = None
USER_CATEGORY_INDEX = 0
KERNEL_CATEGORY_INDEX = 1
class Frame(NamedTuple):
	string_id: StringID
	relevantForJS: bool
	innerWindowID: int
	implementation: None
	optimizations: None
	line: None
	column: None
	category: CategoryID
	subcategory: int
class Stack(NamedTuple):
	prefix_id: Optional[StackID]
	frame_id: FrameID
class Sample(NamedTuple):
	stack_id: Optional[StackID]
	time_ms: Milliseconds
	responsiveness: int
@dataclass
class Thread:
	"""A builder for a profile of the thread.
	Attributes:
		comm: Thread command-line (name).
		pid: process ID of containing process.
		tid: thread ID.
		samples: Timeline of profile samples.
		frameTable: interned stack frame ID -> stack frame.
		stringTable: interned string ID -> string.
		stringMap: interned string -> string ID.
		stackTable: interned stack ID -> stack.
		stackMap: (stack prefix ID, leaf stack frame ID) -> interned Stack ID.
		frameMap: Stack Frame string -> interned Frame ID.
		comm: str
		pid: int
		tid: int
		samples: List[Sample] = field(default_factory=list)
		frameTable: List[Frame] = field(default_factory=list)
		stringTable: List[str] = field(default_factory=list)
		stringMap: Dict[str, int] = field(default_factory=dict)
		stackTable: List[Stack] = field(default_factory=list)
		stackMap: Dict[Tuple[Optional[int], int], int] = field(default_factory=dict)
		frameMap: Dict[str, int] = field(default_factory=dict)
	"""
	comm: str
	pid: int
	tid: int
	samples: List[Sample] = field(default_factory=list)
	frameTable: List[Frame] = field(default_factory=list)
	stringTable: List[str] = field(default_factory=list)
	stringMap: Dict[str, int] = field(default_factory=dict)
	stackTable: List[Stack] = field(default_factory=list)
	stackMap: Dict[Tuple[Optional[int], int], int] = field(default_factory=dict)
	frameMap: Dict[str, int] = field(default_factory=dict)
	def _intern_stack(self, frame_id: int, prefix_id: Optional[int]) -> int:
		"""Gets a matching stack, or saves the new stack. Returns a Stack ID."""
		key = f"{frame_id}" if prefix_id is None else f"{frame_id},{prefix_id}"
		stack_id = self.stackMap.get(key)
		if stack_id is None:
			stack_id = len(self.stackTable)
			self.stackTable.append(Stack(prefix_id=prefix_id, frame_id=frame_id))
			self.stackMap[key] = stack_id
		return stack_id
	def _intern_string(self, string: str) -> int:
		"""Gets a matching string, or saves the new string. Returns a String ID."""
		string_id = self.stringMap.get(string)
		if string_id is not None:
			return string_id
		string_id = len(self.stringTable)
		self.stringTable.append(string)
		self.stringMap[string] = string_id
		return string_id
	def _intern_frame(self, frame_str: str) -> int:
		"""Gets a matching stack frame, or saves the new frame. Returns a Frame ID."""
		frame_id = self.frameMap.get(frame_str)
		if frame_id is not None:
			return frame_id
		frame_id = len(self.frameTable)
		self.frameMap[frame_str] = frame_id
		string_id = self._intern_string(frame_str)
		symbol_name_to_category = KERNEL_CATEGORY_INDEX if frame_str.find('kallsyms') != -1 \
		or frame_str.find('/vmlinux') != -1 \
		or frame_str.endswith('.ko)') \
		else USER_CATEGORY_INDEX
		self.frameTable.append(Frame(
			string_id=string_id,
			relevantForJS=False,
			innerWindowID=0,
			implementation=None,
			optimizations=None,
			line=None,
			column=None,
			category=symbol_name_to_category,
			subcategory=None,
		))
		return frame_id
	def _add_sample(self, comm: str, stack: List[str], time_ms: Milliseconds) -> None:
		"""Add a timestamped stack trace sample to the thread builder.
		Args:
			comm: command-line (name) of the thread at this sample
			stack: sampled stack frames. Root first, leaf last.
			time_ms: timestamp of sample in milliseconds.
		"""
		if self.comm != comm:
			self.comm = comm
		prefix_stack_id = reduce(lambda prefix_id, frame: self._intern_stack
						(self._intern_frame(frame), prefix_id), stack, None)
		if prefix_stack_id is not None:
			self.samples.append(Sample(stack_id=prefix_stack_id,
									time_ms=time_ms,
									responsiveness=0))
	def _to_json_dict(self) -> Dict:
		"""Converts current Thread to GeckoThread JSON format."""
		return {
			"tid": self.tid,
			"pid": self.pid,
			"name": self.comm,
			"markers": {
				"schema": {
					"name": 0,
					"startTime": 1,
					"endTime": 2,
					"phase": 3,
					"category": 4,
					"data": 5,
				},
				"data": [],
			},
			"samples": {
				"schema": {
					"stack": 0,
					"time": 1,
					"responsiveness": 2,
				},
				"data": self.samples
			},
			"frameTable": {
				"schema": {
					"location": 0,
					"relevantForJS": 1,
					"innerWindowID": 2,
					"implementation": 3,
					"optimizations": 4,
					"line": 5,
					"column": 6,
					"category": 7,
					"subcategory": 8,
				},
				"data": self.frameTable,
			},
			"stackTable": {
				"schema": {
					"prefix": 0,
					"frame": 1,
				},
				"data": self.stackTable,
			},
			"stringTable": self.stringTable,
			"registerTime": 0,
			"unregisterTime": None,
			"processType": "default",
		}
def process_event(param_dict: Dict) -> None:
	global start_time
	global tid_to_thread
	time_stamp = (param_dict['sample']['time'] // 1000) / 1000
	pid = param_dict['sample']['pid']
	tid = param_dict['sample']['tid']
	comm = param_dict['comm']
	if not start_time:
		start_time = time_stamp
	stack = []
	if param_dict['callchain']:
		for call in param_dict['callchain']:
			if 'sym' not in call:
				continue
			stack.append(f'{call["sym"]["name"]} (in {call["dso"]})')
		if len(stack) != 0:
			stack = stack[::-1]
	else:
		func = param_dict['symbol'] if 'symbol' in param_dict else '[unknown]'
		dso = param_dict['dso'] if 'dso' in param_dict else '[unknown]'
		stack.append(f'{func} (in {dso})')
	thread = tid_to_thread.get(tid)
	if thread is None:
		thread = Thread(comm=comm, pid=pid, tid=tid)
		tid_to_thread[tid] = thread
	thread._add_sample(comm=comm, stack=stack, time_ms=time_stamp)
def trace_begin() -> None:
	global output_file
	if (output_file is None):
		print("Staring Firefox Profiler on your default browser...")
		global http_server_thread
		http_server_thread = threading.Thread(target=test, args=(CORSRequestHandler, HTTPServer,))
		http_server_thread.daemon = True
		http_server_thread.start()
def trace_end() -> None:
	global output_file
	threads = [thread._to_json_dict() for thread in tid_to_thread.values()]
	gecko_profile_with_meta = {
		"meta": {
			"interval": 1,
			"processType": 0,
			"product": PRODUCT,
			"stackwalk": 1,
			"debug": 0,
			"gcpoison": 0,
			"asyncstack": 1,
			"startTime": start_time,
			"shutdownTime": None,
			"version": 24,
			"presymbolicated": True,
			"categories": CATEGORIES,
			"markerSchema": [],
			},
		"libs": [],
		"threads": threads,
		"processes": [],
		"pausedRanges": [],
	}
	if (output_file is None):
		output_file = 'gecko_profile.json'
		with open(output_file, 'w') as f:
			json.dump(gecko_profile_with_meta, f, indent=2)
		launchFirefox(output_file)
		time.sleep(1)
		print(f'[ perf gecko: Captured and wrote into {output_file} ]')
	else:
		print(f'[ perf gecko: Captured and wrote into {output_file} ]')
		with open(output_file, 'w') as f:
			json.dump(gecko_profile_with_meta, f, indent=2)
class CORSRequestHandler(SimpleHTTPRequestHandler):
	def end_headers (self):
		self.send_header('Access-Control-Allow-Origin', 'https://profiler.firefox.com')
		SimpleHTTPRequestHandler.end_headers(self)
def launchFirefox(file):
	safe_string = urllib.parse.quote_plus(f'http://localhost:8000/{file}')
	url = 'https://profiler.firefox.com/from-url/' + safe_string
	webbrowser.open(f'{url}')
def main() -> None:
	global output_file
	global CATEGORIES
	parser = argparse.ArgumentParser(description="Convert perf.data to Firefox\'s Gecko Profile format which can be uploaded to profiler.firefox.com for visualization")
	parser.add_argument('--user-color', default='yellow', help='Color for the User category', choices=['yellow', 'blue', 'purple', 'green', 'orange', 'red', 'grey', 'magenta'])
	parser.add_argument('--kernel-color', default='orange', help='Color for the Kernel category', choices=['yellow', 'blue', 'purple', 'green', 'orange', 'red', 'grey', 'magenta'])
	parser.add_argument('--save-only', help='Save the output to a file instead of opening Firefox\'s profiler')
	args = parser.parse_args()
	user_color = args.user_color
	kernel_color = args.kernel_color
	output_file = args.save_only
	CATEGORIES = [
		{
			"name": 'User',
			"color": user_color,
			"subcategories": ['Other']
		},
		{
			"name": 'Kernel',
			"color": kernel_color,
			"subcategories": ['Other']
		},
	]
if __name__ == '__main__':
	main()
