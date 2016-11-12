#
#
#              Edit here:
#
#                  |
#                  |
#                  |
#                  V

LANGUAGE_SHORTNAME=de

#
# -----------------------------------------------------------------
# Below this line, no changes should be necessary

PRG:=sap2 sapfs
MO:=$(addsuffix .mo, $(PRG))

PREFIX_MO ?= /usr
MODIR ?= share/locale/$(LANGUAGE_SHORTNAME)/LC_MESSAGES


# Keep secondary targets, especially .po files
.SECONDARY:

.PHONY: i18n clean veryclean install

# Default target:
i18n: sap2.mo sapfs.mo

%.pot: ../../%.c
	@echo Extracting i18n strings from C-Source to pot-file...
	xgettext -c -d $(subst .pot,,$@) -o $@ -k_ -s $<

%.po: %.pot
	if [ ! -f $@ ]; then 			  \
		echo Creating po-file from pot-file...	; \
		cp -v $< $@ 			; \
		sed -ie 's/CHARSET/UTF-8/g' $@ 	; \
	fi
	@echo Merging pot-file into po-file...
	msgmerge $@ $< --update

%.mo: %.po
	@echo Compiling text based po-file to binary mo-file...
	msgfmt $< -o $@

clean:
	rm -f *.pot *.mo *.poe *.po~

veryclean: clean
	rm -f *.po

install:
	install -d $(PREFIX_MO)/$(MODIR)
	install  -m 644 $(MO) $(PREFIX_MO)/$(MODIR)
