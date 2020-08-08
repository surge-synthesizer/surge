#!/bin/bash

WINDIR=buildwin/zip
mkdir -p ${WINDIR}
mkdir -p ${WINDIR}/SurgeData
cd resources/data && tar cf - . | ( cd ../../${WINDIR}/SurgeData && tar xf - )



