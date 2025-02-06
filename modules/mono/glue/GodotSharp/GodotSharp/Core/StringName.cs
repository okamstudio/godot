using System;
using System.Diagnostics.CodeAnalysis;
using Godot.NativeInterop;
using BitFaster.Caching;
using BitFaster.Caching.Lru;

#nullable enable

namespace Godot
{
    /// <summary>
    /// StringNames are immutable strings designed for general-purpose representation of unique names.
    /// StringName ensures that only one instance of a given name exists (so two StringNames with the
    /// same value are the same object).
    /// Comparing them is much faster than with regular strings, because only the pointers are compared,
    /// not the whole strings.
    /// </summary>
    public sealed class StringName : IDisposable, IEquatable<StringName?>
    {
        internal godot_string_name.movable NativeValue;

        private WeakReference<IDisposable>? _weakReferenceToSelf;

        private static readonly ICache<string, StringName> _stringNameCache = new ConcurrentLruBuilder<string, StringName>()
            .WithCapacity(1_000)
            .WithExpireAfterAccess(TimeSpan.FromSeconds(30))
            .Build();

        ~StringName()
        {
            Dispose(false);
        }

        /// <summary>
        /// Disposes of this <see cref="StringName"/>.
        /// </summary>
        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        public void Dispose(bool disposing)
        {
            // Always dispose `NativeValue` even if disposing is true
            NativeValue.DangerousSelfRef.Dispose();

            if (_weakReferenceToSelf != null)
            {
                DisposablesTracker.UnregisterDisposable(_weakReferenceToSelf);
            }
        }

        private StringName(godot_string_name nativeValueToOwn)
        {
            NativeValue = (godot_string_name.movable)nativeValueToOwn;
            _weakReferenceToSelf = DisposablesTracker.RegisterDisposable(this);
        }

        // Explicit name to make it very clear
        internal static StringName CreateTakingOwnershipOfDisposableValue(godot_string_name nativeValueToOwn)
            => new StringName(nativeValueToOwn);

        /// <summary>
        /// Constructs an empty <see cref="StringName"/>.
        /// </summary>
        public StringName()
        {
        }

        /// <summary>
        /// Constructs a <see cref="StringName"/> from the given <paramref name="name"/> string.
        /// </summary>
        /// <param name="name">String to construct the <see cref="StringName"/> from.</param>
        public StringName(string name)
        {
            if (!string.IsNullOrEmpty(name))
            {
                NativeValue = (godot_string_name.movable)NativeFuncs.godotsharp_string_name_new_from_string(name);
                _weakReferenceToSelf = DisposablesTracker.RegisterDisposable(this);
            }
        }

        /// <summary>
        /// Converts a <see cref="string"/> to a <see cref="StringName"/>.<br/>
        /// The resulting <see cref="StringName"/> is temporarily cached for future casts.
        /// </summary>
        /// <param name="from">The string to convert.</param>
        [return: NotNullIfNotNull(nameof(from))]
        public static implicit operator StringName?(string? from)
        {
            if (from is null)
            {
                return null;
            }
            return _stringNameCache.GetOrAdd(from, static (string from) => new StringName(from));
        }

        /// <summary>
        /// Converts a <see cref="StringName"/> to a <see cref="string"/>.
        /// </summary>
        /// <param name="from">The <see cref="StringName"/> to convert.</param>
        [return: NotNullIfNotNull(nameof(from))]
        public static implicit operator string?(StringName? from)
        {
            return from?.ToString();
        }

        /// <summary>
        /// Converts this <see cref="StringName"/> to a string.
        /// </summary>
        /// <returns>A string representation of this <see cref="StringName"/>.</returns>
        public override string ToString()
        {
            if (IsEmpty)
                return string.Empty;

            var src = (godot_string_name)NativeValue;
            NativeFuncs.godotsharp_string_name_as_string(out godot_string dest, src);
            using (dest)
                return Marshaling.ConvertStringToManaged(dest);
        }

        /// <summary>
        /// Check whether this <see cref="StringName"/> is empty.
        /// </summary>
        /// <returns>If the <see cref="StringName"/> is empty.</returns>
        public bool IsEmpty => NativeValue.DangerousSelfRef.IsEmpty;

        public static bool operator ==(StringName? left, StringName? right)
        {
            if (left is null)
                return right is null;
            return left.Equals(right);
        }

        public static bool operator !=(StringName? left, StringName? right)
        {
            return !(left == right);
        }

        public bool Equals([NotNullWhen(true)] StringName? other)
        {
            if (other is null)
                return false;
            return NativeValue.DangerousSelfRef == other.NativeValue.DangerousSelfRef;
        }

        public static bool operator ==(StringName? left, in godot_string_name right)
        {
            if (left is null)
                return right.IsEmpty;
            return left.Equals(right);
        }

        public static bool operator !=(StringName? left, in godot_string_name right)
        {
            return !(left == right);
        }

        public static bool operator ==(in godot_string_name left, StringName? right)
        {
            return right == left;
        }

        public static bool operator !=(in godot_string_name left, StringName? right)
        {
            return !(right == left);
        }

        public bool Equals(in godot_string_name other)
        {
            return NativeValue.DangerousSelfRef == other;
        }

        public override bool Equals([NotNullWhen(true)] object? obj)
        {
            return ReferenceEquals(this, obj) || (obj is StringName other && Equals(other));
        }

        public override int GetHashCode()
        {
            return NativeValue.GetHashCode();
        }
    }
}
