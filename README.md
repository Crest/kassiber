KASSIBER(1) - FreeBSD General Commands Manual

# NAME

**kassiber** - Inject an executable from the FreeBSD host into a jail

# SYNOPSIS

**kassiber**
\[**-h**]
\[**-a**]
\[**-A**]
\[**-n**]
\[**-r**&nbsp;*rtld*]
\[**-p**&nbsp;*path*]
\[**-l**&nbsp;*lib*]
\[**-j**&nbsp;*jail*]
\[**-c**&nbsp;*chroot*]
*cmd*
\[*arg&nbsp;...*]

# DESCRIPTION

The
**kassiber**
command opens the runtime loader and an executable,
loads the required libraries, attaches itself to a jail
and/or chroot directory before executing the loaded command.

The following options are available:

**-h**

> Print the usage message and exit.

**-a**

> Infer the required libraries using the runtime loader's tracing support.
> Put inferred libraries before manually specified libraries.

**-A**

> Infer the required libraries using the runtime loader's tracing support.
> Put inferred libraries after manually specified libraries.

**-n**

> Don't infer the required libraries. Only manually specified libraries will be loaded.

**-r** *rtld*

> Override the the default runtime loader path.

**-l** *lib*

> Manually specify a library to preload. Can be specified more than once.

**-j** *jail*

> Specify the jail to attach to.
> The command attaches to the jail before it chroots itself.

**-c** *chroot*

> Set the chroot directory path.
> The command chroots itself after it attached to the jail.

# CAVEATS

Many programs rely on access to further files besides just their linked
libraries so will be unable to function usefully from within the jail.

# EXIT STATUS

The
**kassiber**
command exits with EX\_NOINPUT, EX\_OSERR, or EX\_CONFIG on errors
otherwise it replaces itself with the invoked executable.

# SEE ALSO

rtld(1),
jexec(8),
chroot(8)

# AUTHORS

This manual page was written by
Jan Bramkamp &lt;crest+kassiber@rlwinm.de&gt;.

FreeBSD 15.0-PRERELEASE - July 12, 2025 - KASSIBER(1)
