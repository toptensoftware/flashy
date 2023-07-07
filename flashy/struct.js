// Typedef is an array of type definitions

function ensureRoom(ctx, required)
{
    // Quit if already fits
    if (ctx.offset + required < ctx.buf.length)
        return;
    
    // Work out new length = at least room for required or double size when small else grow by 64k
    let newLength = Math.max(ctx.buf.length + required, (ctx.buf.length < 65536) ? (ctx.buf.length * 2) : (ctx.buf.length + 65536));
    let newBuf = Buffer.alloc(newLength);
    ctx.buf.copy(newBuf, 0, ctx.length);
    ctx.buf = newBuf;
}

let builtInTypedefs = {
    "uint32le": {
        size: 4,
        gen_encode: (buf, offset, value) => `${buf}.writeUInt32LE(${value}, ${offset})`,
        gen_decode: (buf, offset) => `${buf}.readUInt32LE(${offset})`,
    },
    "uint32be": {
        size: 4,
        gen_encode: (buf, offset, value) => `${buf}.writeUInt32BE(${value}, ${offset})`,
        gen_decode: (buf, offset) => `${buf}.readUInt32BE(${offset})`,
    },
    "uint16le": {
        size: 2,
        gen_encode: (buf, offset, value) => `${buf}.writeUInt16LE(${value}, ${offset})`,
        gen_decode: (buf, offset) => `${buf}.readUInt16LE(${offset})`,
    },
    "uint16be": {
        size: 2,
        gen_encode: (buf, offset, value) => `${buf}.writeUint16BE(${value}, ${offset})`,
        gen_decode: (buf, offset) => `${buf}.readUInt16BE(${offset})`,
    },
    "uint8": {
        size: 1,
        gen_encode: (buf, offset, value) => `${buf}.writeUInt8(${value}, ${offset})`,
        gen_decode: (buf, offset) => `${buf}.readUInt8(${offset})`,
    },
    "int32le": {
        size: 4,
        gen_encode: (buf, offset, value) => `${buf}.writeInt32LE(${value}, ${offset})`,
        gen_decode: (buf, offset) => `${buf}.readInt32LE(${offset})`,
    },
    "int32be": {
        size: 4,
        gen_encode: (buf, offset, value) => `${buf}.writeInt32BE(${value}, ${offset})`,
        gen_decode: (buf, offset) => `${buf}.readInt32BE(${offset})`,
    },
    "int16le": {
        size: 2,
        gen_encode: (buf, offset, value) => `${buf}.writeInt16LE(${value}, ${offset})`,
        gen_decode: (buf, offset) => `${buf}.readInt16LE(${offset})`,
    },
    "int16be": {
        size: 2,
        gen_encode: (buf, offset, value) => `${buf}.writeInt16BE(${value}, ${offset})`,
        gen_decode: (buf, offset) => `${buf}.readInt16BE(${offset})`,
    },
    "int8": {
        size: 1,
        gen_encode: (buf, offset, value) => `${buf}.writeInt8(${value}, ${offset})`,
        gen_decode: (buf, offset) => `${buf}.readInt8(${offset})`,
    },

    "floatbe": {
        size: 4,
        gen_encode: (buf, offset, value) => `${buf}.writeFloatBE(${value}, ${offset})`,
        gen_decode: (buf, offset) => `${buf}.readFloatBE(${offset})`,
    },

    "floatle": {
        size: 4,
        gen_encode: (buf, offset, value) => `${buf}.writeFloatLE(${value}, ${offset})`,
        gen_decode: (buf, offset) => `${buf}.readFloatLE(${offset})`,
    },

    "doublebe": {
        size: 8,
        gen_encode: (buf, offset, value) => `${buf}.writeDoubleBE(${value}, ${offset})`,
        gen_decode: (buf, offset) => `${buf}.readDoubleBE(${offset})`,
    },

    "doublele": {
        size: 8,
        gen_encode: (buf, offset, value) => `${buf}.writeDoubleLE(${value}, ${offset})`,
        gen_decode: (buf, offset) => `${buf}.readDoubleLE(${offset})`,
    },

    "string": {
        gen_pre_encode: (buf, offset, value) => `let tmp = Buffer.from(${value}, 'utf8');\nctx.ensureRoom(tmp.length + 1);`,
        gen_encode: (buf, offset, value) => `tmp.copy(${buf}, ${offset});`,
        gen_post_encode: (buf, offset, value) => `${offset} += tmp.length + 1;`,

        gen_pre_decode: (buf, offset) => `tmp = ${buf}.indexOf(0, ${offset});`,
        gen_decode: (buf, offset) => `${buf}.subarray(${offset}, tmp).toString('utf8');`,
        gen_post_decode: (buf, offset) => `${offset} = tmp + 1;`,
    }

}

function parseFieldDefinition(def)
{
    let m = def.match(/^([a-zA-Z0-9$_]+)(?:\[([a-zA-Z0-9$_]+)\])?\s*([a-zA-Z0-9$_]+)?/);

    // Try to convert count to integer
    let count = parseInt(m[2]);
    if (isNaN(count))
    {
        count = m[2];
    }

    return {
        type: m[1],
        count: count,
        name: m[3],
    }
}


function code_builder()
{
    let code = "";
    let indentStr = ``;

    function append_line(str)
    {
        if (str === null)
            return;
        code += `${indentStr}${str}\n`;
    }

    function append(str)
    {
        for (let l of str.split('\n'))
        {
            append_line(l);
        }
    }

    function indent()
    {
        indentStr += '    ';
    }

    function unindent()
    {
        indentStr = indentStr.substring(0, indentStr.length - 4);
    }

    function finish()
    {
        return code;
    }

    return { append, append_line, indent, unindent, finish };
}


function gen_decode(lib, type)
{
    let code = code_builder();
    code.indent();

    code.append(`let struct = {};`);
    code.append(`let tmp;`);
    code.append(`let array;`);

    let decoders_used = {};
    for (let fd of type.fields)
    {
        let ft = lib.getType(fd.type);

        // Struct type?
        if (ft.decode && !decoders_used[ft.type])
        {
            code.append(`let type_${ft.name}_decode = lib.getType('${ft.name}').decode;`);
            decoders_used[ft.name] = true;
        }
    }


    for (let fd of type.fields)
    {
        // Find the type
        let td = lib.getType(fd.type);

        function gen_pre_decode()
        {
            if (td.gen_pre_decode)
                code.append(td.gen_pre_decode('ctx.buf', 'ctx.offset'));
        }

        function gen_decode_value()
        {
            if (td.gen_decode)
                return td.gen_decode('ctx.buf', 'ctx.offset');
            if (td.decode)
                return `type_${fd.type}_decode(lib, ctx)`;
        }

        function gen_post_decode()
        {
            if (td.gen_post_decode)
                code.append(td.gen_post_decode('ctx.buf', 'ctx.offset'));

            if (td.size && td.gen_decode)
                code.append(`ctx.offset += ${td.size};`);
        }

        // Decode
        let value;
        if (fd.count !== undefined)
        {
            // How many?
            if (typeof(fd.count) == 'number')
            {
                count = `${count}`;
            }
            else
            {
                if (fd.count.startsWith('$'))
                    count = fd.count;
                else
                    count = `struct[${fd.count}]`;
            }

            code.append(`array = [];`);
            code.append(`for (let j=0; j<${count}; j++)`);
            code.append(`{`);
            code.indent();
            gen_pre_decode();
            code.append(`array.push(${gen_decode_value()});`);
            gen_post_decode();
            code.unindent();
            code.append(`}`);

            if (fd.name.startsWith('$'))
                code.append(`${fd.name} = array;`)
            else
                code.append(`struct['${fd.name}'] = array;`)
        }
        else
        {
            gen_pre_decode();
            if (fd.name.startsWith('$'))
                code.append(`let ${fd.name} = ${gen_decode_value()};`)
            else
                code.append(`struct['${fd.name}'] = ${gen_decode_value()};`)
            gen_post_decode();
        }
    }

    code.append(`return struct;`)

    return new Function(['lib', 'ctx'], code.finish());
}

function calc_room_ensurance(lib, fields, from)
{
    let size = 0;
    let sizeExprs = [];
    let i;
    for (i=from; i<fields.length; i++)
    {
        let field = fields[i];
        let ft = lib.getType(field.type);

        // Variable size?
        if (ft.size === undefined)
            break;
        
        // Handle arrays
        if (field.count === undefined)
        {
            size += ft.size;
        }
        else if (typeof(field.count) == 'number')
        {
            size += ft.size * fields.count;
        }
        else
        {
            let countField = field.name.startsWith('$') ? field.name : `struct['${field.name}'].length`;
            sizeExprs.push(`${countField} * ${ft.size}`);
        }
    }

    if (size == 0 && sizeExprs.length == 0)
        return null;

    if (sizeExprs.length == 0)
    {
        return {
            ensuredToField: i,
            ensureExpression: `${size}`,
        }
    }
    else if (size == 0)
    {
        return {
            ensuredToField: i,
            ensureExpression: `${sizeExprs.join(" + ")}`,
        }
    }
    else
    {
        return {
            ensuredToField: i,
            ensureExpression: `${size} + ${sizeExprs.join(" + ")}`,
        }
    }
}

function gen_encode(lib, type)
{
    let code = code_builder();
    code.indent();

    code.append('let temp;');
    code.append('let array;');

    // Work out room currently ensured
    let ensuredToField = 0;
    if (type.size)
    {
        // Fixed size, all fields already ensured
        ensuredToField = type.fields.length;
    }

    let encoders_used = {};
    for (let fd of type.fields)
    {
        // Transient?
        if (typeof(fd.count) == 'string' && fd.count.startsWith('$'))
        {
            code.append(`let ${fd.count} = struct['${fd.name}'].length;`);
        }

        let td = lib.getType(fd.type);

        // Struct type?
        if (td.encode && !encoders_used[td.type])
        {
            code.append(`let type_${td.name}_encode = lib.getType('${td.name}').encode;`);
            encoders_used[td.name] = true;
        }
    }


    // Generate all fields
    for (let i=0; i<type.fields.length; i++)
    {
        let fd = type.fields[i];
        let ft = lib.getType(fd.type);

        // Do we need to ensure room again?
        if (i == ensuredToField)
        {
            // Calculate ensurance
            let ensurance = calc_room_ensurance(lib, type.fields, i);
            if (ensurance)
            {
                // Ensure room and remember where we're ensured to
                ensuredToField = ensurance.ensuredToField;
                code.append(`ctx.ensureRoom(${ensurance.ensureExpression});`)
            }
            else
            {
                // Try again next field
                ensuredToField = i+1;
            }
        }

        
        function gen_pre_encode(value)
        {
            if (ft.gen_pre_encode)
                code.append(ft.gen_pre_encode('ctx.buf', 'ctx.offset', value));
        }

        function gen_encode(value)
        {
            if (ft.gen_encode)
                code.append(ft.gen_encode('ctx.buf', 'ctx.offset', value) + ";");
            if (ft.encode)
                code.append(`type_${fd.type}_encode(lib, ctx, ${value});`);
        }

        function gen_post_encode(value)
        {
            if (ft.gen_post_encode)
                code.append(ft.gen_post_encode('ctx.buf', 'ctx.offset', value));

            if (ft.size && ft.gen_decode)
                code.append(`ctx.offset += ${ft.size};`);
        }

        if (fd.count !== undefined)
        {
            // How many?
            let count = parseInt(fd.count);
            if (typeof(fd.count) == 'number')
            {
                count = `${count}`;
            }
            else
            {
                if (fd.count.startsWith('$'))
                    count = fd.count;
                else
                    count = `struct[${fd.count}]`;
            }

            code.append(`array = struct['${fd.name}'];`);
            code.append(`for (let j=0; j<${count}; j++)`);
            code.append(`{`);
            code.indent();
            gen_pre_encode(`array[j]`);
            gen_encode(`array[j]`)
            gen_post_encode(`array[j]`);
            code.unindent();
            code.append(`}`);
        }
        else
        {
            gen_pre_encode(`struct['${fd.name}']`);
            if (fd.name.startsWith('$'))
                gen_encode(fd.name);
            else
                gen_encode(`struct['${fd.name}']`);
            gen_post_encode(`struct['${fd.name}']`);
        }
    }

    return new Function(['lib', 'ctx', 'struct'], code.finish());
}



function library()
{
    let lib = {};
    lib.types = {};

    lib.getType = function(name)
    {
        // Built in type?
        let td = builtInTypedefs[name];
        if (!td)
            td = lib.types[name];
        if (!td)
            throw new Error(`Unknown type '${name}'`);
        return td;
    }

    lib.defineType = function(typedef)
    {
        // Parse field definitions
        let fds = typedef.fields.map(x => parseFieldDefinition(x));
        
        // Calculate fixed size
        let size = 0;
        for (let fd of fds)
        {
            // Get type
            let td = lib.getType(fd.type);

            // If variable size, can't work out fixed sie
            if (td.size === undefined)
            {
                size = undefined;
                break;
            }

            // Is it an array
            if (fd.count !== undefined)
            {
                if (typeof(fd.count) == 'number')
                {
                    // fixed size arrary
                    size += td.size * fd.count;
                }
                else
                {
                    // variable size array
                    size = undefined;
                    break;
                }
            }
            else
            {
                size += td.size;
            }
        }

        // Register it
        let td = {
            name: typedef.name,
            fields: fds,
            typedef,
            size,
        };
        
        // generate encoder and decoder
        td.decode = gen_decode(lib, td);
        td.encode = gen_encode(lib, td);

        lib.types[typedef.name] = td;
    }

    lib.decode = function(typename, buf)
    {
        return lib.getType(typename).decode(lib, {
            buf,
            offset: 0
        });
    }

    lib.encode = function(typename, struct)
    {
        let type = lib.getType(typename);
        let ctx = {
            buf: Buffer.alloc(type.size === undefined ? 32 : type.size),
            offset: 0,
            ensureRoom: (required) => ensureRoom(ctx, required),
        };
        type.encode(lib, ctx, struct);
        return ctx.buf.subarray(0, ctx.offset);
    }

    return lib;
}


module.exports = {
    library
};
