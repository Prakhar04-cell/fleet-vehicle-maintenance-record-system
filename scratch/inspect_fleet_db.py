import sqlite3

con = sqlite3.connect("data/fleet.db")
cur = con.cursor()
tables = [r[0] for r in cur.execute("SELECT name FROM sqlite_master WHERE type='table'").fetchall()]
print("Tables in data/fleet.db:", tables)
for t in ["users", "vehicles", "maintenance_records", "expenses", "audit_logs"]:
    if t in tables:
        count = cur.execute(f"SELECT count(*) FROM {t}").fetchone()[0]
        print(f"  {t}: {count} records")
con.close()
