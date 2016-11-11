#     Edit here:
#
#         |
#         |
#         |
#         V

LANGUAGES=de fr

# -----------------------------------------------------------------------


.PHONY: i18n clean install

i18n:
	for l in $(LANGUAGES); do make -C $$l; done

clean:
	for l in $(LANGUAGES); do make -C $$l clean; done

install:
	for l in $(LANGUAGES); do make -C $$l install; done
