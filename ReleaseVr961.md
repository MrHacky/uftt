# What changed and what is new in UFTT release 961 #

UFTT release 961 represents a major leap forward in file-transfer
technology. Many causes of (potential) crashes and erroneous
behaviour have been fixed (including rigorous testing of numerous corner-cases),
ensuring that UFTT release 961 is the most stable version of UFTT released
to date. Not only is UFTT now more stable than ever before, also the
user-interface
design has seen many small changes and improvements. UFTT is now easier to
use than ever, with an improved, intuitive, user-interface. This does not
mean that UFTT
Care has been taken
to maintain the UFTT `look and feel', so that anyone who has used UFTT
before will feel right at home.


  * A new, GTK-based, user-interface was added. For Linux users this means   that it is now possible to use UFTT in GTK/Gnome based distributions without needing to install the Qt-libraries, thus saving a considerable ammount of space on your harddisk. Unfortunately it is currently necessary to compile UFTT yourself (either from the SVN repository or from the source tar-ball).

  * Many improvements to both the GTK and QT based user-interface. Both  interfaces now have nearly identical feature sets. The main differences  with respect to the [r610](https://code.google.com/p/uftt/source/detail?r=610) Qt user interface are:
    * The download-path input-box now interactively checks the validity of the entered download path, alerting you to an invalid download location by colouring red.
    * The user-interface now retains a list of the most recently used     download-paths, allowing for quicker selection of a suitable download     location.
> > > Note: the GTK-based user-interface leverages GTK's default
> > > file-selection dialog, giving instant access to all your `Places'.
    * It is now possible for UFTT to notify you of the completion of a     download by means of a pop-up. Under windows this is implemented using     the default windows notification system (i.e. a task-tray balloon),     whereas under Linux UFTT relies on the services of libnotify. (This     does mean that you will need to run some sort of notification-manager     if you wish to see the notifications. Most popular Linux distributions     include a notification-manager by default.)
    * Both the share-list and the task-list now provide access to a right-click menu, giving access to the most commonly used actions.
    * It is finally possible to remove a share from the share-list without requiring a full restart of UFTT. (Cancelling an in-progress download still requires a full restart though.)
    * The menus in the Qt-based user-interface were getting a bit cluttered with options. A new preferences dialog was implemented to replace the existing options, and provide a way to implement more complex ways to configure UFTT.
    * All commonly used commands and menu-items now support accelerators     (keyboard-shortcuts) for quick and easy access.
    * We have greatly improved the handling of multiple UFTT-instances.     It used to be the case that if you already had an instance of UFTT     open (for example minimised to the task-tray), starting a second     instance of UFTT would fail with a confusing error message. Now UFTT will activate (bring-to-front) the already activate instance.
    * Along with the previous change we have improved UFTT's handling of     commandline arguments. It is now possible to specify any number of     files and directory on the commandline, which will be added to your     share-list. Further it is now possible /download/ a share using the     commandline, too.


  * The accuracy of the tracking and reporting of the amount of up- and  downloaded data has been increased, giving you an even better view of  your transfer situation and more accurate time estimates.
  * Not only has the accuracy of tracking and reporting been improved, there have also been various tweaks to the transferring algorithm which now uses an adaptive buffer size. This means that UFTT will now show the  progress of a transfer as early as after transferring the first kilo-byte  of data, whereas before a transfer would not appear to have started until after the first 10 mega-bytes of data had been transferred. What this means is that you get much more frequent, more accurate estimates of the transfer-time estimates.

  * UFTT now has experimental support for Mac OS. An (older) beta-build is available in our downloads area. Unfortunately due to time-constraints this beta-release is not as polished as our usual releases. Although it works fine for transferring files, there a number of small issues, mostly to do with the interface and integration into MacOS environment (such as the task-tray-icon look and its behaviour towards minimising to either the tray or the MacOS-dock.). We are currently looking for a developer willing to take on the task of improving UFTT's MacOS support. If you'd like to help out, please open a ticket in our bug-tracker.