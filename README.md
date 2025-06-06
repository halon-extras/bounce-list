# Bounce list plugin

This plugin allows you to define custom bounce lists which may be checked from HSL (```bounce_list(id, diagnosticcode [, grouping [, state]])```).

```
halonctl plugin command bounce-list reload list1
```

## Installation

Follow the [instructions](https://docs.halon.io/manual/comp_install.html#installation) in our manual to add our package repository and then run the below command.

### Ubuntu

```
apt-get install halon-extras-bounce-list
```

### RHEL

```
yum install halon-extras-bounce-list
```

## Configuration

For the configuration schema, see [bounce-list.schema.json](bounce-list.schema.json).

### smtpd.yaml

```
plugins:
  - id: bounce-list
    config:
      lists:
        - id: list1
          path: /etc/halon/list1.csv
```

### Bounce list format

The following syntax is supported in the bounce list file.

```
/550 5.7.1 Email quota exceeded\./,other,&google,EOD
/550 5.7.1 Email quota exceeded\./,other,&google
/550 5.7.1\./,other,"&hotmail,&office365"
/.*/,other
```

Where the first column is the bounce pattern (PCRE, caseless matching), the second is the returned value if there is a match, the third is the grouping (support list of groupings separated by comma), the fourth is the SMTP state and the fifth is a comment. Lines starting with `#` are treated as comments.

## Exported commands

The default is to auto reload bounce-list files when the configuration changes (`halonctl config reload`). However they can also be be reloaded manually using the halonctl command.

```
halonctl plugin command bounce-list reload list1
```

It's possible to test if an SMTP response diagnostic code matches a bounce pattern in a bounce list by issuing this command

```
halonctl plugin command bounce-list test list1 "550 5.7.1 Email quota exceeded." "&google" "EOD"
```

## Exported functions

These functions needs to be [imported](https://docs.halon.io/hsl/structures.html#import) from the `extras://bounce-list` module path.

### bounce_list(id, diagnosticcode [, grouping [, state]])

Check if an SMTP response diagnostic code matches a bounce pattern.

**Params**

- id `string` - The bounce list ID
- diagnosticcode `string` - The diagnostic code (`$arguments["dsn"]["diagnosticcode"]`)
- grouping `string` - The grouping (`$arguments["grouping"]`)
- state `string` - The SMTP state (`$arguments["attempt"]["result"]["state"]`)

_Grouping_ enables you to restrict the scope of a rule to a specific named grouping (such as a list of rolled-up destinations.)

_State_ enables you to restrict the scope of a rule to a specific point in the SMTP conversation, such as `EOD` - see [here](https://docs.halon.io/hsl/postdelivery.html?highlight=attempt%20result%20state#smtp-states) for all values.

**Returns**

Returns an `array` with the index of `pattern` and `value` if there is a match otherwise `none`. On error an exception is thrown.

**Example (Post-delivery)**

```
import { bounce_list } from "extras://bounce-list";

if ($arguments["action"]) {
    echo bounce_list("list1", $arguments["dsn"]["diagnosticcode"], $arguments["grouping"], $arguments["attempt"]["result"]["state"]);
}
```
