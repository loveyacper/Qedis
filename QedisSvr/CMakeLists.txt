PROJECT(QEDISSERVER)

INCLUDE(../CMakeCommon)

AUX_SOURCE_DIRECTORY(. QEDISSERVER_SRC)

LINK_DIRECTORIES(../../leveldb)
INCLUDE_DIRECTORIES(../QBase; ../QedisCore)

ADD_EXECUTABLE(qedis_server ${QEDISSERVER_SRC})
SET(EXECUTABLE_OUTPUT_PATH  ../../bin)

TARGET_LINK_LIBRARIES(qedis_server qediscore; qbaselib; leveldb)
ADD_DEPENDENCIES(qedis_server qbaselib; qediscore; leveldb)
