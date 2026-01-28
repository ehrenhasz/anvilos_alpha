import sqlite3
import uuid
import time
import sys
import os
SYSTEM_DB = "/var/lib/anvilos/db/cortex.db"
def submit_goal(goal_text):
    goal_id = str(uuid.uuid4())
    print(f"Submitting goal: {goal_text} (ID: {goal_id})")
    with sqlite3.connect(SYSTEM_DB) as conn:
        conn.execute("INSERT INTO sys_goals (id, goal, stat, timestamp) VALUES (?, ?, 0, ?)",
                     (goal_id, goal_text, time.time()))
    return goal_id
if __name__ == "__main__":
    goal = "Create a file named 'hello_mainframe.txt' with content 'I AM THE ENGINE' and then list the directory."
    if len(sys.argv) > 1:
        goal = sys.argv[1]
    submit_goal(goal)
