# From log file to coordinates (PPK)

Each session produces `GCP-01_Snnnn.rtcm` (raw data) and `.txt` (metadata).

1. Convert to RINEX with RTKLIB (demo5 build). RTCM carries no date, so pass
   the approximate UTC start from the `.txt` file:
   `convbin -r rtcm3 -tr 2026/11/17 07:12:00 -v 3.04 -od -os -o gcp01.obs -n gcp01.nav GCP-01_S0003.rtcm`
2. Download RINEX for the same time span from a nearby CORS station.
3. Process in RTKPOST, mode **Static**, base = CORS, rover = `gcp01.obs`.
   (Or upload the RINEX file to NRCan CSRS-PPP / OPUS for an independent check.)
4. Apply the antenna reference point to target-surface height offset.
