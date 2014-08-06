RUNTIME_OCR_SRC=ocr

noinst_LTLIBRARIES += libruntime_ocr.la
libhclib_la_LIBADD += libruntime_ocr.la

RT_OCR_LDFLAGS=$(OCR_LDFLAGS) $(OCR_LIBS)
RT_OCR_CFLAGS=-I$(OCR_ROOT)/include

libruntime_ocr_la_SOURCES = \
$(RUNTIME_OCR_SRC)/runtime-ocr.c

libruntime_ocr_la_CFLAGS = $(CFLAGS) $(INCLUDES_DIR) $(RT_OCR_CFLAGS)
libruntime_ocr_la_LDFLAGS = $(RT_OCR_LDFLAGS)