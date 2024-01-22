set(simbricks_path $ENV{SIMBRICKS_PATH})

build_lib(
  LIBNAME ptp
  SOURCE_FILES
    model/ptp-queue.cc
  HEADER_FILES
    model/ptp-queue.h
)