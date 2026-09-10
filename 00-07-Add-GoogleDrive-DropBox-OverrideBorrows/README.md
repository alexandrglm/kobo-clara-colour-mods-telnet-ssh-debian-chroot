# Enable Hidden Kobo Features II
## Google Drive, DropBox, and Override borrow system

Modern Kobo firmware still contains unused, hidden system applications and services that can be enabled with a few configuration tweaks. This guide covers the **cloud storage integrations** (Google Drive, Dropbox) and the **OverDrive borrow books system**.

These features can be launched by using `NickelMenu`, or `NickelDBus`. While they can also be triggered using **KFmon** alongside PNG trigger files, using **NickelMenu** is the cleanest, most efficient method if you already have it installed.

Once configured, sync folder paths are available at:

*   DropBox:        `/mnt/onboard/.kobo/dropbox`
*   Google Drive:   `/mnt/onboard/.kobo/google_drive`

*   Your public library system added via Override will be integrated into My Books options (as well as GDrive and Dropbox) 

---

### 1:  Enable the Features in `Kobo eReader.conf`

Edit the file located at:

```
/mnt/onboard/.kobo/Kobo/Kobo eReader.conf
```

> [!IMPORTANT]
> Only **add** the lines shown below. Do **not** remove, reorder, or modify any existing entries. The sections listed here are shown for context only, leave everything else untouched.

Add the following lines to the appropriate sections. If a section already exists, simply append the new keys beneath it.

```ini
[General]

# <DO NOT EDIT HERE>

[ApplicationPreferences]
# ...
BorrowDialogShown=false
GoogleDriveEnabled=true
OverDriveEnabled=True
OverDriveFilterShown=false
# ...

[BluetoothSettings]

# <DO NOT EDIT HERE>

[Browser]

# <DO NOT EDIT HERE>

[DeveloperSettings]

# ...
EnableDebugServices=True
EnableFeatureSettings=True
# ...

[DialogSettings]

# <DO NOT EDIT HERE>

[DropboxSettings]

# DO NOT ADD THESE, WILL BE AUTOMATICALLY ADDED ON FIRST LOGIN
# AccessToken=
# UserGuideId=
# Username=

[FeatureSettings]
# ...
DevelopInventions=true
DropboxEnabled=True
GoogleDriveEnabled=True
OverDriveEnabled=True
OverDriveShowQuickTour=False
# ...

[GDriveSettings]

# DO NOT ADD THESE, WILL BE AUTOMATICALLY ADDED ON FIRST LOGIN
# RootAppFolderId=
# UserGuideId=

[Instapaper]

# <DO NOT EDIT HERE>

[NickelMenu]

# <DO NOT EDIT HERE>

[OneStoreServices]
# ...
dropbox_link_account_poll=https://authorize.kobo.com/{region}/{language}/LinkDropbox
googledrive_link_account_start=https://authorize.kobo.com/{region}/{language}/LinkGoogleDrive
kobo_dropbox_link_account_enabled=True
kobo_googledrive_link_account_enabled=True
kobo_nativeborrow_enabled=True
# ...

[PowerOptions]

# <DO NOT EDIT HERE>

[ReadingLife]

# <DO NOT EDIT HERE>

[ReadingOptions]

# <DO NOT EDIT HERE>

[RushHour]

# <DO NOT EDIT HERE>

[Services]
OverDrive=PRIME
```

Save the file and safely eject the device.

---

### 2.  Launch the Features

Once the configuration is in place, the hidden services will not appear in **Settings** by default. There are several ways to reach them:

- **NickelDBus** via `qndb` commands.
- **KFmon** using a PNG trigger file for each feature.
- **NickelMenu** the recommended approach (see below).

### NickelMenu entries

Add the following lines to your NickelMenu configuration file (e.g. `/mnt/onboard/.adds/nm/cloud` or any `.nm` file):

```text
menu_item :main :GDrive    :nickel_open :library:gdrive
menu_item :main :DropBox   :nickel_open :library:dropbox
menu_item :main :Overdrive :nickel_open :store:overdrive
```

Reboot the device to apply the changes.

---

> [!IMPORTANT]
> **OverDrive** is the exception: it **will** appear in **Settings** **but only on the **first reboot** after adding the keys above. This is your one and only chance to add the library you wish to use with the OverDrive override system. If you miss this window, you may need to re-apply the relevant keys and reboot again to bring the option back.

---
|

Enjoy your newly unlocked Kobo!
