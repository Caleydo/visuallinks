visuallinks
===========

Context Preserving Visual Links and Hidden Content Visualization 


Dependencies
------------

qt-sdk, openCL

Building
--------

Install dependencies:

  $ apt-get install qt-sdk

Checkout sources:

  $ git clone https://github.com/Caleydo/visuallinks.git

In the repository's folder get submodules (qtwebsocket):

  $ git submodule init
  $ git submodule update

Create build directory and check for dependencies:

  $ mkdir build && cd build
  $ cmake ..

Change settings (eg. disable GPU routing) with ccmake:

  $ ccmake ..

Install firefox addon:

Create a file named 'werner.puff@gmx.net' inside your Firefox extension directory
with a single line containing the full path to <repo>/addons/firefox/src

Alternatively just install <repo>/addons/firefox/src/src.xpi as addon. With this
method the addon will not be automatically updated.

After installing the add-on you will need to add the icon that hooks firefox up to the visual links application to the toolbar.


Run '''qtfullscreensystem''':

  $ cd bin/qtfullscreensystem
  $ ./qtfullscreensystem qtconfig.xml
  
Then connect your browser by clicking the visual links icon. 

To trigger a visual link select a word and click the icon again.
