/* Pi-hole: A black hole for Internet advertisements
*  (c) 2017 Pi-hole, LLC (https://pi-hole.net)
*  Network-wide ad blocking via your own hardware.
*
*  FTL Engine
*  Garbage collection routines
*
*  This file is copyright under the latest version of the EUPL.
*  Please see LICENSE file for your rights under this license. */

#include "FTL.h"

void *GC_thread(void *val)
{
	// Set thread name
	prctl(PR_SET_NAME,"GC",0,0,0);

	// Lock FTL's data structure, since it is likely that it will be changed here
	// Requests should not be processed/answered when data is about to change
	enable_lock("GC_thread");

	// Get minimum time stamp to keep
	int mintime = time(NULL) - GCdelay - MAXLOGAGE;
	if(debugGC)
	{
		time_t timestamp = mintime;
		logg_str("GC all queries older than: ", strtok(ctime(&timestamp),"\n"));
	}

	// Process all queries
	int i;
	for(i=0; i < counters.queries; i++)
	{
		if(queries[i].timestamp < mintime && queries[i].valid)
		{
			// Adjust total counters and total over time data
			// We cannot edit counters.queries directly as it is used
			// as max ID for the queries[] struct
			counters.invalidqueries++;
			overTime[queries[i].timeidx].total--;

			// Adjust client and domain counters
			clients[queries[i].clientID].count--;
			domains[queries[i].domainID].count--;
			forwarded[queries[i].forwardID].count--;

			// Change other counters according to status of this query
			switch(queries[i].status)
			{
				case 0: counters.unknown--; break;
				case 1: counters.blocked--; overTime[queries[i].timeidx].blocked--; domains[queries[i].domainID].blockedcount--; break;
				case 2: break;
				case 3: counters.cached--; break;
				case 4: counters.wildcardblocked--; overTime[queries[i].timeidx].blocked--; break;
				default: /* That cannot happen */ break;
			}

			// Mark this query as garbage collected
			queries[i].valid = false;

			if(debugGC)
			{
				time_t timestamp = queries[i].timestamp;
				logg_str("GC query with time: ", strtok(ctime(&timestamp),"\n"));
			}
		}
	}

	if(debugGC)
	{
		logg_int("GC queries: ", counters.invalidqueries);
	}

	// Release thread lock
	disable_lock("GC_thread");


	return NULL;
}