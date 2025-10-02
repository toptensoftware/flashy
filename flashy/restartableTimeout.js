class RestartableTimeout
{
    constructor(callback, period)
    {
        this.period = period;
        this.callback = callback;
        this.timeout = setTimeout(callback, period);
    }

    cancel()
    {
        if (this.timeout)
        {
            clearTimeout(this.timeout);
            this.timeout = null;
        }
    }

    restart(period)
    {
        // Change period?
        if (period !== undefined)
            this.period = period;

        // Cancel old timeout
        this.cancel();

        // Setup new timeout
        this.timeout = setTimeout(this.callback, this.period);
    }
}

export default RestartableTimeout;