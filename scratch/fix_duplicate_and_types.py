with open('src/http_server.c', 'r', encoding='utf-8') as f:
    code = f.read()

# Marker 1 is at line 332
m1 = code.find('OPERATIONAL DSA INTEGRATION HANDLERS')
assert m1 != -1, "m1 not found"
# Back up to the start of the comment block
comment_start = code.rfind('/*', 0, m1)

# Find end of first block: right before static void handle_api_vehicles
next_fn = 'static void handle_api_vehicles(SOCKET client'
end1 = code.find(next_fn, m1)
assert end1 != -1, "next_fn not found"

# Remove first block
code = code[:comment_start] + code[end1:]
print("[OK] Removed first duplicate block")

# Fix field and enum names
code = code.replace('sched.target_odometer_km', 'sched.due_odometer_km')
code = code.replace('list[i].year', 'list[i].manufacturing_year')
code = code.replace('VEHICLE_SORT_YEAR', 'VEHICLE_SORT_REG')

with open('src/http_server.c', 'w', encoding='utf-8') as f:
    f.write(code)

print("[SUCCESS] Fixed duplicate and types in src/http_server.c")
