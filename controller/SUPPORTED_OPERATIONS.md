# GDPR-Compliant Key-Value Store Operations

## GET Operation
**What it does**: Gets one piece of data by its unique identifier

1. Look up the data using the key you provided
2. Check if the data has expired (is it still valid to access?)
3. Check if you're allowed to see this data (are you the owner or has it been shared with you?)
4. Check if your intended use matches what the data is allowed to be used for
5. Check if the data owner has objected to your intended use
6. If all checks pass, return the actual data (without the privacy metadata)
7. Log this access attempt for audit purposes

---

## PUT Operation  
**What it does**: Saves new data or updates existing data while keeping privacy rules

1. Check if data with this key already exists
2. If it exists, check if you own it (only owners can update their data)
3. If you're authorized, create the new version:
   - For new data: add fresh privacy metadata based on your permissions
   - For existing data: keep the old privacy rules, just update the actual data
4. Save the updated data
5. Update the system's quick-lookup cache if needed
6. Log this storage operation

---

## DELETE Operation
**What it does**: Permanently removes data (owner-only operation)

1. Check if the data exists
2. Verify you are the owner (sharing doesn't matter for deletion - only owners can delete)
3. Check if the data hasn't expired yet
4. If you're authorized, permanently delete the data from storage
5. Remove it from the quick-lookup cache too
6. Log this deletion for audit trail

---

## PUTC Operation 
**What it does**: Completely replaces both data and privacy rules (owner-only)

1. Check if you own the existing data (if any)
2. Verify your permissions for complete control
3. Create entirely new data with completely new privacy metadata from scratch
4. Replace everything - both the actual data and all privacy rules
5. Update the cache with the new privacy information
6. Log this complete replacement

---

## GETM Operation
**What it does**: Gets multiple pieces of data based on search criteria

1. Find all data that starts with your search term (like finding all files starting with "photo_")
2. For each piece of data found:
   - **First filter**: Does it match your specific criteria? (owned by certain users, used for certain purposes, etc.)
   - **Second check**: Are you allowed to access it? (ownership, sharing, intended use, objections, expiration)
3. If requesting "data": return the actual content without privacy metadata
4. If requesting "metadata": return only the privacy information (owner-only)
5. Combine all allowed results with separators
6. Log every access attempt

---

## PUTM Operation
**What it does**: Updates privacy metadata for multiple data entries that match criteria

1. Find all data that starts with your search term
2. For each piece of data found:
   - **First filter**: Does it match your specific criteria?
   - **Second check**: Do you own this data? (only owners can change privacy metadata)
3. For data you're allowed to modify:
   - Update the privacy metadata with your new rules
   - Keep the actual data unchanged
4. Save all changes in one batch operation
5. Count and report how many succeeded vs failed
6. Log all modification attempts

---

## Key Differences Summary

| Aspect | Single Operations (GET/PUT/DELETE/PUTC) | Bulk Operations (GETM/PUTM) |
|--------|----------------------------------------|----------------------------|
| **Scope** | Work on one specific item | Work on many items at once |
| **Search** | Direct key lookup | Pattern matching with prefix |
| **Filtering** | No filtering step | Two-phase: filter criteria → permission check |
| **Performance** | Fast individual operations | Efficient batch processing |

### Permission Levels

| Operation Type | Who Can Access |
|----------------|----------------|
| **Viewing data** | Owner OR people it's shared with |
| **Viewing privacy info** | Owner only |
| **Changing data** | Owner only |
| **Changing privacy rules** | Owner only |
| **Deleting** | Owner only |

### Filter vs Permission Check (for GETM/PUTM)

| Phase | Purpose | Example Criteria |
|-------|---------|------------------|
| **Filter** | "Which items should we consider?" | `sessionKeyIs`, `objPurIs`, `objExpIs` |
| **Permission** | "Is this user allowed to access/modify?" | Ownership, sharing, expiration, objections |
