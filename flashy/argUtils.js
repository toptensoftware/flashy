let path = require('path');
let fs = require('fs');
let os = require('os');

function glob_internal(filename, pattern, ixf, ixp, caseSensitive)
{
    // Compare characters
    while (true)
    {
        // End of both strings = match!
        if (ixf == filename.length && ixp == pattern.length)
            return true;

        // Single character wildcard
        if (pattern[ixp] == '?')
        {
            ixf++;
            ixp++;
            continue;
        }

        if (pattern[ixp] == '*')
        {
            ixp++;

            // If nothing after the wildcard then match
            if (ixp == pattern.length)
                return true;

            while (ixf < filename.length)
            {
                if (glob_internal(filename, pattern, ixf, ixp, caseSensitive))
                    return true;
                ixf++;
            }
            return false;
        }

        // Handle case sensitivity
        let cf = filename[ixf];
        let cp = pattern[ixp];

        if (!caseSensitive)
        {
            cf = cf.toUpperCase();
            cp = cp.toUpperCase();
        }
        
        // Characters match
        if (cp != cf)
            return false;

        ixf++;
        ixp++;
    }
}

// Simple glob pattern matcher (supports ? and *), no special 
// handling for / \ or .
function glob(filename, pattern, caseSensitive)
{
    return glob_internal(filename, pattern, 0, 0, caseSensitive);
}

function pathiswild(arg)
{
    return arg.indexOf('?') >= 0 || arg.indexOf('*') >= 0;
}


function expandArg(arg)
{
    // If not a wildcard, just stat it and done
    if (!pathiswild(arg))
    {
        try
        {
            let stat = fs.statSync(arg);
            return [ { relative: arg, stat } ]
        }
        catch
        {
            return [ { relative: arg, stat: null } ]
        }
    }

    // Find matching files
    let files = [];
    let dirname = path.dirname(arg);
    let spec = path.basename(arg);
    let dir = fs.opendirSync(dirname);
    try
    {
        let caseSensitive = os.platform() != 'win32';
        while (true)
        {
            let de = dir.readSync();
            if (!de)
                break;
    
            if (!glob(de.name, spec, caseSensitive))
                continue;
                
            let relpath = path.join(dirname, de.name);
            files.push({
                relative: relpath,
                stat: fs.statSync(relpath)
            })
        }
    
        // If no matches, use arg as is
        if (files.length == 0)
        {
            files.push({
                relative: arg,
                stat: null,
            });
        }
    
        // Done!
        return files;
    }
    finally
    {
        dir.closeSync();
    }
}

// Expand a list of filename arguments
function expandArgs(args)
{
    let r = [];
    for (let a of args)
    {
        r = r.concat(expandArg(a));
    }
    return r;
}

function pathjoin(a, b)
{
    // Convert to forward slashes
    a = a.replace(/\\/g, '/');
    b = b.replace(/\\/g, '/');

    // Is b fully qualified?
    if (b.match("/^[a-zA-Z0-9]+\:") || b.startsWith('/'))
        return b;

    if (!a.endsWith('/'))
        return a + '/' + b;
    else
        return a + b;
}



module.exports = {
    glob,
    expandArg,
    expandArgs,
    pathjoin,
}