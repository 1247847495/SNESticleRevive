#!/bin/bash
echo "--- app source in container? ---"
ls -d /tmp/SNESticle* /src/SNESticle* /root/SNESticle* /opt/SNESticle* 2>/dev/null
find /tmp -maxdepth 2 -name 'Makefile' -path '*SNES*' 2>/dev/null | head
find / -maxdepth 3 -name 'SNESticleAurora*' -not -path '/proc/*' 2>/dev/null | head
echo "--- build dir? ---"
find /tmp /src /root -maxdepth 4 -name 'SNESticle.elf' 2>/dev/null | head
