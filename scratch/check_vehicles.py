import sqlite3

con = sqlite3.connect("data/fleet.db")
cur = con.cursor()
cur.execute("SELECT id, registration_number, make, model FROM vehicles")
print("Vehicles in data/fleet.db:")
for row in cur.fetchall():
    print(row)
