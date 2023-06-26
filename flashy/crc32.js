let g_crcTable;

function crc32_init()
{
    if (g_crcTable)
        return;

    g_crcTable = [];

	// This is the official polynomial used by CRC-32
	// in PKZip, WinZip and Ethernet.
	let ulPolynomial = 0x04c11db7;

	// 256 values representing ASCII character codes.
	for (let i = 0; i <= 0xFF; i++)
	{
        let e = reflect(i, 8) << 24;
		for (let j = 0; j < 8; j++)
			e = ((e << 1) ^ (e & (1 << 31) ? ulPolynomial : 0)) >>> 0;
        g_crcTable.push(reflect(e, 32) >>> 0);
	}

    function reflect(ref, ch)
    {
        let value = 0;

        // Swap bit 0 for bit 7
        // bit 1 for bit 6, etc.
        for (let i = 1; i < (ch + 1); i++)
        {
            if(ref & 1)
                value |= 1 << (ch - i) >>> 0;
            ref >>= 1;
        }
    
        return value;
    }
}


crc32_init();


function crc32_start()
{
    return 0xFFFFFFFF;
}

function crc32_update(crc, byte)
{
    return ((crc >>> 8) ^ g_crcTable[(crc & 0xFF) ^ (byte & 0xFF)]) >>> 0;
}

function crc32_finish(crc)
{
	return (crc ^ 0xffffffff) >>> 0;
}

function crc32_string(str)
{
    let crc = crc32_start();
    for (let i=0; i<str.length; i++)
    {
        crc = crc32_update(crc, str.charCodeAt(i));
    }
    return crc32_finish(crc);
}

function crc32_buffer(buf)
{
    let crc = crc32_start();
    for (let i=0; i<buf.length; i++)
    {
        crc = crc32_update(crc, buf[i]);
    }
    return crc32_finish(crc);
}

module.exports = {
    start: crc32_start,
    finish: crc32_finish,
    update: crc32_update,
    string: crc32_string,
    buffer: crc32_buffer,
}
