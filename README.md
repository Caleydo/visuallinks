# Visual Links to Hidden Content

Context Preserving Visual Links and Hidden Content Visualization


## Dependencies

qt-sdk

Install dependencies:

    $ apt-get install qt-sdk

## Building

Checkout sources:

    $ git clone https://github.com/Caleydo/visuallinks.git

In the repository's folder get submodules (qtwebsocket):

    $ git submodule init
    $ git submodule update

Create build directory and check for dependencies:

    $ mkdir build && cd build
    $ cmake ..

Change settings (eg. enable building search widget) with ccmake:

    $ ccmake ..

## Install firefox add-on

**Warning**: Firefox 26 seems to have a bug which causes rendering artifacts for the smart preview. With Firefox 27 this problem does not occur anymore (Older versions of Firefox also do not show this problem).

### Install from file

1. Get Firefox [add-on file](https://github.com/Caleydo/visuallinks/blob/master/addons/firefox/hidden-content.xpi?raw=true)
2. Within Firefox navigate to [about:addons](about:addons)
3. In the Tools menu select "Install Add-on From File"
4. Select the downloaded file for installation

### Manual installation

Create a file named 'hidden-content@caleydo.org' inside your Firefox extension directory with a single line containing the full path to <your-source-directory>/addons/firefox/src

## Run the server

Run '''qtfullscreensystem''':

    $ cd bin/qtfullscreensystem
    $ ./qtfullscreensystem qtconfig.xml

## Use Firefox addon

1. Add "Hidden Content" button to Navigation Toolbar
  1. Select "Customize" in Navigation Toolbar context menu
  2. Drag "Hidden Content" button to the Navigation Toolbar
2. Connect to Hidden Content server (Click on "Hidden Content" button)
3. Initiate new linking process
  1. Select a word or phrase
  2. Press "Hidden Content" button or alternatively press [CTRL]

# Troubleshooting

Maximum size of shared memory segment exceeded (indicated by the following error
messages):

    QNativeImage: Unable to attach to shared memory segment.
    QPainter::begin: Paint device returned engine == 0, type: 3
    QPainter::setCompositionMode: Painter not active

Increase maximum shared memory segment size:

    $ # Immediate change:
    $ echo "67108864" >/proc/sys/kernel/shmmax

    $ # Permanent change:
    $ echo "kernel.shmmax=67108864" >/etc/sysctl.d/90-shared-memory.conf
