# Aggregator for the test binaries.
# Each sub-pro builds independently.

TEMPLATE = subdirs
CONFIG  += ordered

SUBDIRS = \
    conversions \
    compare \
    engine
