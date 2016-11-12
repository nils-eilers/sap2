#     Edit here:
#
#         |
#         |
#         |
#         V

LANGUAGES=de fr

# -----------------------------------------------------------------------

PREFIX_MO ?= /usr

.PHONY: i18n clean install

i18n:
	for l in $(LANGUAGES); do make -C $$l; done

clean:
	for l in $(LANGUAGES); do make -C $$l clean; done

install:
	for l in $(LANGUAGES); do make -C $$l install PREFIX_MO=$(PREFIX_MO); done
