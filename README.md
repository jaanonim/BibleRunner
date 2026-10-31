> [!CAUTION]
> Archived - use [BibleRunner-rs](https://github.com/jaanonim/BibleRunner-rs) instead.

# Bible Runner

Bible Runner is plugin for KRunner that allows you to quickly open verse in YouVersion Bible.

## Installation

Install dependencies:

```bash
apt install libkf5kio-dev ibkf5notifications-dev cmake make x-www-browser
```

Run install script:

```bash
./install.sh
```

## Configuration

Verses are now fetched through the official YouVersion Platform API, so a free app key is required. Create an app at https://platform.youversion.com (free for non-commercial use).

Create the config file `~/.config/biblerunner.json` by hand (GUI configuration is planned later; directory can be overridden with `BIBLERUNNER_CONFIG_PATH` env var):

```json
{
    "app_key": "PASTE_YOUR_APP_KEY",
    "bible_id": 3034
}
```

- `app_key` - the YouVersion Platform app key (sent as the `X-YVP-App-Key` header)
- `bible_id` - numeric id of the bible translation to use (e.g. `3034` = Berean Standard Bible)

Without a valid config file the plugin only opens verses in the browser (default translation `2095`) and the copy-verse match stays disabled.

## Copyright

This project uses [YouVersion Bible](https://www.bible.com/).
Icon from: https://www.freeiconspng.com/img/18585
