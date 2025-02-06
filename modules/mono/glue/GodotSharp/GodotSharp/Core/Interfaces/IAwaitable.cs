namespace Godot
{
    /// <summary>
    /// An interface that requires a GetAwaiter() method to get a reference to the Awaiter.
    /// </summary>
    public interface IAwaitable
    {
        /// <summary>
        /// Gets an Awaiter for this <see cref="IAwaitable"/>.
        /// </summary>
        /// <returns>An Awaiter.</returns>
        IAwaiter GetAwaiter();
    }

    /// <summary>
    /// A templated interface that requires a GetAwaiter() method to get a reference to the Awaiter.
    /// </summary>
    /// <typeparam name="TResult">A reference to the result to be passed out.</typeparam>
    public interface IAwaitable<out TResult>
    {
        /// <summary>
        /// Gets an Awaiter for this <see cref="IAwaitable{TResult}"/>.
        /// </summary>
        /// <returns>An Awaiter.</returns>
        IAwaiter<TResult> GetAwaiter();
    }
}
