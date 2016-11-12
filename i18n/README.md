How to start a new translation for a further language
-----------------------------------------------------

- Edit the Makefile in this directory and add the two or three
  letter code for the new language as listed in
  /usr/share/i18n/SUPPORTED such as "de" or "fr"
- Create a new subfolder with this language code as its name
- Copy a Makefile from any other supported language into the new empty
  subfolder for new new language
- Edit the new Makefile and change the language code (e.g. "de" --> "fr")
- Type "make" to let the Makefile create an empty set of string files
- Fill the generated file sap2.po with translated strings or give it
  to your translator
- Type "make" to compile the translated strings from the po-file to
  a binary mo-file for use by the program
- Type "sudo make install" to move the binary mo-file to a place where
  the program expects it



Updating a translation
-----------------------

- Edit the po-file to update a translation, then run "make" followed
  by "sudo make install" to apply the new strings.
- Also, when the program changes and introduces new strings or removes
  old ones, just run "make" too to update the po-file for the new strings.
  Edit your po-file then with the new translations and run "make" followed
  by "sudo make install" again.

