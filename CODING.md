some coding conventions:

* headers in include/core_api shall not include headers from other
  YafRay include dirs.
* headers in include/core_api shall not use HAVE_* options
  (like '#if HAVE_EXR')
* plugins using headers other than core_api ones need to have all
  libs added to their build environment for all HAVE_* options they
  pull in (directly or indirectly!)
* interface functions shall not have arguments or return types which
  depend on configurable typedefines (such as float, PLOAT and float)
* use doxygen comments to document non-obvious types, classes, interfaces
  and functions and their usage.

## formatting
* use tabs, not spaces
* recommended tab width is 4
* 80 character terminals are from last century...
  everyone should be able to view at least 120 character long lines

