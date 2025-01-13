using System;
using System.Collections.Generic;
using System.Collections;
using System.Diagnostics.CodeAnalysis;
using System.Runtime.CompilerServices;
using Godot.NativeInterop;
using System.Diagnostics;

#nullable enable

namespace Godot.Collections
{
    /// <summary>
    /// Wrapper around Godot's Dictionary class, a dictionary of Variant
    /// typed elements allocated in the engine in C++. Useful when
    /// interfacing with the engine.
    /// </summary>
    [DebuggerTypeProxy(typeof(DictionaryDebugView<Variant, Variant>))]
    [DebuggerDisplay("Count = {Count}")]
    public sealed class Dictionary :
        IDictionary<Variant, Variant>,
        IReadOnlyDictionary<Variant, Variant>,
        IDisposable
    {
        internal godot_dictionary.movable NativeValue;

        private WeakReference<IDisposable>? _weakReferenceToSelf;

        /// <summary>
        /// Constructs a new empty <see cref="Dictionary"/>.
        /// </summary>
        public Dictionary()
        {
            NativeValue = (godot_dictionary.movable)NativeFuncs.godotsharp_dictionary_new();
            _weakReferenceToSelf = DisposablesTracker.RegisterDisposable(this);
        }

        private Dictionary(godot_dictionary nativeValueToOwn)
        {
            NativeValue = (godot_dictionary.movable)(nativeValueToOwn.IsAllocated ?
                nativeValueToOwn :
                NativeFuncs.godotsharp_dictionary_new());
            _weakReferenceToSelf = DisposablesTracker.RegisterDisposable(this);
        }

        // Explicit name to make it very clear
        internal static Dictionary CreateTakingOwnershipOfDisposableValue(godot_dictionary nativeValueToOwn)
            => new Dictionary(nativeValueToOwn);

        ~Dictionary()
        {
            Dispose(false);
        }

        /// <summary>
        /// Disposes of this <see cref="Dictionary"/>.
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

        /// <summary>
        /// Returns a copy of the <see cref="Dictionary"/>.
        /// If <paramref name="deep"/> is <see langword="true"/>, a deep copy is performed:
        /// all nested arrays and dictionaries are duplicated and will not be shared with
        /// the original dictionary. If <see langword="false"/>, a shallow copy is made and
        /// references to the original nested arrays and dictionaries are kept, so that
        /// modifying a sub-array or dictionary in the copy will also impact those
        /// referenced in the source dictionary. Note that any <see cref="GodotObject"/> derived
        /// elements will be shallow copied regardless of the <paramref name="deep"/>
        /// setting.
        /// </summary>
        /// <param name="deep">If <see langword="true"/>, performs a deep copy.</param>
        /// <returns>A new Godot Dictionary.</returns>
        public Dictionary Duplicate(bool deep = false)
        {
            godot_dictionary newDictionary;
            var self = (godot_dictionary)NativeValue;
            NativeFuncs.godotsharp_dictionary_duplicate(ref self, deep.ToGodotBool(), out newDictionary);
            return CreateTakingOwnershipOfDisposableValue(newDictionary);
        }

        /// <summary>
        /// Adds entries from <paramref name="dictionary"/> to this dictionary.
        /// By default, duplicate keys are not copied over, unless <paramref name="overwrite"/>
        /// is <see langword="true"/>.
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// The dictionary is read-only.
        /// </exception>
        /// <param name="dictionary">Dictionary to copy entries from.</param>
        /// <param name="overwrite">If duplicate keys should be copied over as well.</param>
        public void Merge(Dictionary dictionary, bool overwrite = false)
        {
            ThrowIfReadOnly();

            var self = (godot_dictionary)NativeValue;
            var other = (godot_dictionary)dictionary.NativeValue;
            NativeFuncs.godotsharp_dictionary_merge(ref self, in other, overwrite.ToGodotBool());
        }

        /// <summary>
        /// Compares this <see cref="Dictionary"/> against the <paramref name="other"/>
        /// <see cref="Dictionary"/> recursively. Returns <see langword="true"/> if the
        /// two dictionaries contain the same keys and values. The order of the entries
        /// does not matter.
        /// otherwise.
        /// </summary>
        /// <param name="other">The other dictionary to compare against.</param>
        /// <returns>
        /// <see langword="true"/> if the dictionaries contain the same keys and values,
        /// <see langword="false"/> otherwise.
        /// </returns>
        public bool RecursiveEqual(Dictionary other)
        {
            var self = (godot_dictionary)NativeValue;
            var otherVariant = (godot_dictionary)other.NativeValue;
            return NativeFuncs.godotsharp_dictionary_recursive_equal(ref self, otherVariant).ToBool();
        }

        // IDictionary

        /// <summary>
        /// Gets the collection of keys in this <see cref="Dictionary"/>.
        /// </summary>
        public ICollection<Variant> Keys
        {
            get
            {
                godot_array keysArray;
                var self = (godot_dictionary)NativeValue;
                NativeFuncs.godotsharp_dictionary_keys(ref self, out keysArray);
                return Array.CreateTakingOwnershipOfDisposableValue(keysArray);
            }
        }

        /// <summary>
        /// Gets the collection of elements in this <see cref="Dictionary"/>.
        /// </summary>
        public ICollection<Variant> Values
        {
            get
            {
                godot_array valuesArray;
                var self = (godot_dictionary)NativeValue;
                NativeFuncs.godotsharp_dictionary_values(ref self, out valuesArray);
                return Array.CreateTakingOwnershipOfDisposableValue(valuesArray);
            }
        }

        IEnumerable<Variant> IReadOnlyDictionary<Variant, Variant>.Keys => Keys;

        IEnumerable<Variant> IReadOnlyDictionary<Variant, Variant>.Values => Values;

        private (Array keys, Array values, int count) GetKeyValuePairs()
        {
            var self = (godot_dictionary)NativeValue;

            godot_array keysArray;
            NativeFuncs.godotsharp_dictionary_keys(ref self, out keysArray);
            var keys = Array.CreateTakingOwnershipOfDisposableValue(keysArray);

            godot_array valuesArray;
            NativeFuncs.godotsharp_dictionary_values(ref self, out valuesArray);
            var values = Array.CreateTakingOwnershipOfDisposableValue(valuesArray);

            int count = NativeFuncs.godotsharp_dictionary_count(ref self);

            return (keys, values, count);
        }

        /// <summary>
        /// Returns the value at the given <paramref name="key"/>.
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// The property is assigned and the dictionary is read-only.
        /// </exception>
        /// <exception cref="KeyNotFoundException">
        /// The property is retrieved and an entry for <paramref name="key"/>
        /// does not exist in the dictionary.
        /// </exception>
        /// <value>The value at the given <paramref name="key"/>.</value>
        public Variant this[Variant key]
        {
            get
            {
                var self = (godot_dictionary)NativeValue;

                if (NativeFuncs.godotsharp_dictionary_try_get_value(ref self,
                        (godot_variant)key.NativeVar, out godot_variant value).ToBool())
                {
                    return Variant.CreateTakingOwnershipOfDisposableValue(value);
                }
                else
                {
                    throw new KeyNotFoundException();
                }
            }
            set
            {
                ThrowIfReadOnly();

                var self = (godot_dictionary)NativeValue;
                NativeFuncs.godotsharp_dictionary_set_value(ref self,
                    (godot_variant)key.NativeVar, (godot_variant)value.NativeVar);
            }
        }

        /// <summary>
        /// Adds an value <paramref name="value"/> at key <paramref name="key"/>
        /// to this <see cref="Dictionary"/>.
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// The dictionary is read-only.
        /// </exception>
        /// <exception cref="ArgumentException">
        /// An entry for <paramref name="key"/> already exists in the dictionary.
        /// </exception>
        /// <param name="key">The key at which to add the value.</param>
        /// <param name="value">The value to add.</param>
        public void Add(Variant key, Variant value)
        {
            ThrowIfReadOnly();

            var variantKey = (godot_variant)key.NativeVar;
            var self = (godot_dictionary)NativeValue;

            if (NativeFuncs.godotsharp_dictionary_contains_key(ref self, variantKey).ToBool())
                throw new ArgumentException("An element with the same key already exists.", nameof(key));

            godot_variant variantValue = (godot_variant)value.NativeVar;
            NativeFuncs.godotsharp_dictionary_add(ref self, variantKey, variantValue);
        }

        void ICollection<KeyValuePair<Variant, Variant>>.Add(KeyValuePair<Variant, Variant> item)
            => Add(item.Key, item.Value);

        /// <summary>
        /// Clears the dictionary, removing all entries from it.
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// The dictionary is read-only.
        /// </exception>
        public void Clear()
        {
            ThrowIfReadOnly();

            var self = (godot_dictionary)NativeValue;
            NativeFuncs.godotsharp_dictionary_clear(ref self);
        }

        /// <summary>
        /// Checks if this <see cref="Dictionary"/> contains the given key.
        /// </summary>
        /// <param name="key">The key to look for.</param>
        /// <returns>Whether or not this dictionary contains the given key.</returns>
        public bool ContainsKey(Variant key)
        {
            var self = (godot_dictionary)NativeValue;
            return NativeFuncs.godotsharp_dictionary_contains_key(ref self, (godot_variant)key.NativeVar).ToBool();
        }

        bool ICollection<KeyValuePair<Variant, Variant>>.Contains(KeyValuePair<Variant, Variant> item)
        {
            godot_variant variantKey = (godot_variant)item.Key.NativeVar;
            var self = (godot_dictionary)NativeValue;
            bool found = NativeFuncs.godotsharp_dictionary_try_get_value(ref self,
                variantKey, out godot_variant retValue).ToBool();

            using (retValue)
            {
                if (!found)
                    return false;

                godot_variant variantValue = (godot_variant)item.Value.NativeVar;
                return NativeFuncs.godotsharp_variant_equals(variantValue, retValue).ToBool();
            }
        }

        /// <summary>
        /// Removes an element from this <see cref="Dictionary"/> by key.
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// The dictionary is read-only.
        /// </exception>
        /// <param name="key">The key of the element to remove.</param>
        public bool Remove(Variant key)
        {
            ThrowIfReadOnly();

            var self = (godot_dictionary)NativeValue;
            return NativeFuncs.godotsharp_dictionary_remove_key(ref self, (godot_variant)key.NativeVar).ToBool();
        }

        bool ICollection<KeyValuePair<Variant, Variant>>.Remove(KeyValuePair<Variant, Variant> item)
        {
            ThrowIfReadOnly();

            godot_variant variantKey = (godot_variant)item.Key.NativeVar;
            var self = (godot_dictionary)NativeValue;
            bool found = NativeFuncs.godotsharp_dictionary_try_get_value(ref self,
                variantKey, out godot_variant retValue).ToBool();

            using (retValue)
            {
                if (!found)
                    return false;

                godot_variant variantValue = (godot_variant)item.Value.NativeVar;
                if (NativeFuncs.godotsharp_variant_equals(variantValue, retValue).ToBool())
                {
                    return NativeFuncs.godotsharp_dictionary_remove_key(
                        ref self, variantKey).ToBool();
                }

                return false;
            }
        }

        /// <summary>
        /// Returns the number of elements in this <see cref="Dictionary"/>.
        /// This is also known as the size or length of the dictionary.
        /// </summary>
        /// <returns>The number of elements.</returns>
        public int Count
        {
            get
            {
                var self = (godot_dictionary)NativeValue;
                return NativeFuncs.godotsharp_dictionary_count(ref self);
            }
        }

        /// <summary>
        /// Returns <see langword="true"/> if the dictionary is read-only.
        /// See <see cref="MakeReadOnly"/>.
        /// </summary>
        public bool IsReadOnly => NativeValue.DangerousSelfRef.IsReadOnly;

        /// <summary>
        /// Makes the <see cref="Dictionary"/> read-only, i.e. disabled modying of the
        /// dictionary's elements. Does not apply to nested content, e.g. content of
        /// nested dictionaries.
        /// </summary>
        public void MakeReadOnly()
        {
            if (IsReadOnly)
            {
                // Avoid interop call when the dictionary is already read-only.
                return;
            }

            var self = (godot_dictionary)NativeValue;
            NativeFuncs.godotsharp_dictionary_make_read_only(ref self);
        }

        /// <summary>
        /// Gets the value for the given <paramref name="key"/> in the dictionary.
        /// Returns <see langword="true"/> if an entry for the given key exists in
        /// the dictionary; otherwise, returns <see langword="false"/>.
        /// </summary>
        /// <param name="key">The key of the element to get.</param>
        /// <param name="value">The value at the given <paramref name="key"/>.</param>
        /// <returns>If an entry was found for the given <paramref name="key"/>.</returns>
        public bool TryGetValue(Variant key, out Variant value)
        {
            var self = (godot_dictionary)NativeValue;
            bool found = NativeFuncs.godotsharp_dictionary_try_get_value(ref self,
                (godot_variant)key.NativeVar, out godot_variant retValue).ToBool();

            value = found ? Variant.CreateTakingOwnershipOfDisposableValue(retValue) : default;

            return found;
        }

        /// <summary>
        /// Copies the elements of this <see cref="Dictionary"/> to the given untyped
        /// <see cref="KeyValuePair{TKey, TValue}"/> array, starting at the given index.
        /// </summary>
        /// <exception cref="ArgumentNullException">
        /// The <paramref name="array"/> is <see langword="null"/>.
        /// </exception>
        /// <exception cref="ArgumentOutOfRangeException">
        /// <paramref name="arrayIndex"/> is less than 0 or greater than the array's size.
        /// </exception>
        /// <exception cref="ArgumentException">
        /// The destination array was not long enough.
        /// </exception>
        /// <param name="array">The array to copy to.</param>
        /// <param name="arrayIndex">The index to start at.</param>
        void ICollection<KeyValuePair<Variant, Variant>>.CopyTo(KeyValuePair<Variant, Variant>[] array, int arrayIndex)
        {
            if (array == null)
                throw new ArgumentNullException(nameof(array), "Value cannot be null.");

            if (arrayIndex < 0)
                throw new ArgumentOutOfRangeException(nameof(arrayIndex),
                    "Number was less than the array's lower bound in the first dimension.");

            var (keys, values, count) = GetKeyValuePairs();

            if (array.Length < (arrayIndex + count))
                throw new ArgumentException(
                    "Destination array was not long enough. Check destIndex and length, and the array's lower bounds.");

            for (int i = 0; i < count; i++)
            {
                array[arrayIndex] = new(keys[i], values[i]);
                arrayIndex++;
            }
        }

        // IEnumerable

        /// <summary>
        /// Gets an enumerator for this <see cref="Dictionary"/>.
        /// </summary>
        /// <returns>An enumerator.</returns>
        public IEnumerator<KeyValuePair<Variant, Variant>> GetEnumerator()
        {
            for (int i = 0; i < Count; i++)
            {
                yield return GetKeyValuePair(i);
            }
        }

        IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();

        private KeyValuePair<Variant, Variant> GetKeyValuePair(int index)
        {
            var self = (godot_dictionary)NativeValue;
            NativeFuncs.godotsharp_dictionary_key_value_pair_at(ref self, index,
                out godot_variant key,
                out godot_variant value);
            return new KeyValuePair<Variant, Variant>(Variant.CreateTakingOwnershipOfDisposableValue(key),
                Variant.CreateTakingOwnershipOfDisposableValue(value));
        }

        /// <summary>
        /// Converts this <see cref="Dictionary"/> to a string.
        /// </summary>
        /// <returns>A string representation of this dictionary.</returns>
        public override string ToString()
        {
            var self = (godot_dictionary)NativeValue;
            NativeFuncs.godotsharp_dictionary_to_string(ref self, out godot_string str);
            using (str)
                return Marshaling.ConvertStringToManaged(str);
        }

        private void ThrowIfReadOnly()
        {
            if (IsReadOnly)
            {
                throw new InvalidOperationException("Dictionary instance is read-only.");
            }
        }
    }

    internal interface IGenericGodotDictionary
    {
        public Dictionary UnderlyingDictionary { get; }
    }

    /// <summary>
    /// Typed wrapper around Godot's Dictionary class, a dictionary of Variant
    /// typed elements allocated in the engine in C++. Useful when
    /// interfacing with the engine. Otherwise prefer .NET collections
    /// such as <see cref="System.Collections.Generic.Dictionary{TKey, TValue}"/>.
    /// </summary>
    /// <typeparam name="TKey">The type of the dictionary's keys.</typeparam>
    /// <typeparam name="TValue">The type of the dictionary's values.</typeparam>
    [DebuggerTypeProxy(typeof(DictionaryDebugView<,>))]
    [DebuggerDisplay("Count = {Count}")]
    [SuppressMessage("Design", "CA1001", MessageId = "Types that own disposable fields should be disposable",
            Justification = "Known issue. Requires explicit refcount management to not dispose untyped collections.")]
    public class Dictionary<[MustBeVariant] TKey, [MustBeVariant] TValue> :
        IDictionary<TKey, TValue>,
        IReadOnlyDictionary<TKey, TValue>,
        IGenericGodotDictionary
    {
        private static godot_variant ToVariantFunc(scoped in Dictionary<TKey, TValue> godotDictionary) =>
            VariantUtils.CreateFromDictionary(godotDictionary);

        private static Dictionary<TKey, TValue> FromVariantFunc(in godot_variant variant) =>
            VariantUtils.ConvertToDictionary<TKey, TValue>(variant);

        private void SetTypedForUnderlyingDictionary()
        {
            Marshaling.GetTypedCollectionParameterInfo<TKey>(out var keyVariantType, out var keyClassName, out var keyScriptRef);
            Marshaling.GetTypedCollectionParameterInfo<TValue>(out var valueVariantType, out var valueClassName, out var valueScriptRef);

            var self = (godot_dictionary)NativeValue;

            using (keyScriptRef)
            using (valueScriptRef)
            {
                NativeFuncs.godotsharp_dictionary_set_typed(
                    ref self,
                    (uint)keyVariantType,
                    keyClassName,
                    keyScriptRef,
                    (uint)valueVariantType,
                    valueClassName,
                    valueScriptRef);
            }
        }

        static unsafe Dictionary()
        {
            VariantUtils.GenericConversion<Dictionary<TKey, TValue>>.ToVariantCb = ToVariantFunc;
            VariantUtils.GenericConversion<Dictionary<TKey, TValue>>.FromVariantCb = FromVariantFunc;
        }

        private readonly Dictionary _underlyingDict;

        Dictionary IGenericGodotDictionary.UnderlyingDictionary => _underlyingDict;

        internal ref godot_dictionary.movable NativeValue
        {
            [MethodImpl(MethodImplOptions.AggressiveInlining)]
            get => ref _underlyingDict.NativeValue;
        }

        /// <summary>
        /// Constructs a new empty <see cref="Dictionary{TKey, TValue}"/>.
        /// </summary>
        /// <returns>A new Godot Dictionary.</returns>
        public Dictionary()
        {
            _underlyingDict = new Dictionary();
            SetTypedForUnderlyingDictionary();
        }

        /// <summary>
        /// Constructs a new <see cref="Dictionary{TKey, TValue}"/> from the given dictionary's elements.
        /// </summary>
        /// <exception cref="ArgumentNullException">
        /// The <paramref name="dictionary"/> is <see langword="null"/>.
        /// </exception>
        /// <param name="dictionary">The dictionary to construct from.</param>
        /// <returns>A new Godot Dictionary.</returns>
        public Dictionary(IDictionary<TKey, TValue> dictionary)
        {
            ArgumentNullException.ThrowIfNull(dictionary);

            _underlyingDict = new Dictionary();
            SetTypedForUnderlyingDictionary();

            foreach (KeyValuePair<TKey, TValue> entry in dictionary)
                Add(entry.Key, entry.Value);
        }

        /// <summary>
        /// Constructs a new <see cref="Dictionary{TKey, TValue}"/> from the given dictionary's elements.
        /// </summary>
        /// <exception cref="ArgumentNullException">
        /// The <paramref name="dictionary"/> is <see langword="null"/>.
        /// </exception>
        /// <param name="dictionary">The dictionary to construct from.</param>
        /// <returns>A new Godot Dictionary.</returns>
        public Dictionary(Dictionary dictionary)
        {
            ArgumentNullException.ThrowIfNull(dictionary);

            _underlyingDict = dictionary;
        }

        // Explicit name to make it very clear
        internal static Dictionary<TKey, TValue> CreateTakingOwnershipOfDisposableValue(
            godot_dictionary nativeValueToOwn)
            => new Dictionary<TKey, TValue>(Dictionary.CreateTakingOwnershipOfDisposableValue(nativeValueToOwn));

        /// <summary>
        /// Converts this typed <see cref="Dictionary{TKey, TValue}"/> to an untyped <see cref="Dictionary"/>.
        /// </summary>
        /// <param name="from">The typed dictionary to convert.</param>
        /// <returns>A new Godot Dictionary, or <see langword="null"/> if <see paramref="from"/> was null.</returns>
        [return: NotNullIfNotNull("from")]
        public static explicit operator Dictionary?(Dictionary<TKey, TValue>? from)
        {
            return from?._underlyingDict;
        }

        /// <summary>
        /// Returns a copy of the <see cref="Dictionary{TKey, TValue}"/>.
        /// If <paramref name="deep"/> is <see langword="true"/>, a deep copy is performed:
        /// all nested arrays and dictionaries are duplicated and will not be shared with
        /// the original dictionary. If <see langword="false"/>, a shallow copy is made and
        /// references to the original nested arrays and dictionaries are kept, so that
        /// modifying a sub-array or dictionary in the copy will also impact those
        /// referenced in the source dictionary. Note that any <see cref="GodotObject"/> derived
        /// elements will be shallow copied regardless of the <paramref name="deep"/>
        /// setting.
        /// </summary>
        /// <param name="deep">If <see langword="true"/>, performs a deep copy.</param>
        /// <returns>A new Godot Dictionary.</returns>
        public Dictionary<TKey, TValue> Duplicate(bool deep = false)
        {
            return new Dictionary<TKey, TValue>(_underlyingDict.Duplicate(deep));
        }

        /// <summary>
        /// Adds entries from <paramref name="dictionary"/> to this dictionary.
        /// By default, duplicate keys are not copied over, unless <paramref name="overwrite"/>
        /// is <see langword="true"/>.
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// The dictionary is read-only.
        /// </exception>
        /// <param name="dictionary">Dictionary to copy entries from.</param>
        /// <param name="overwrite">If duplicate keys should be copied over as well.</param>
        public void Merge(Dictionary<TKey, TValue> dictionary, bool overwrite = false)
        {
            _underlyingDict.Merge(dictionary._underlyingDict, overwrite);
        }

        /// <summary>
        /// Compares this <see cref="Dictionary{TKey, TValue}"/> against the <paramref name="other"/>
        /// <see cref="Dictionary{TKey, TValue}"/> recursively. Returns <see langword="true"/> if the
        /// two dictionaries contain the same keys and values. The order of the entries does not matter.
        /// otherwise.
        /// </summary>
        /// <param name="other">The other dictionary to compare against.</param>
        /// <returns>
        /// <see langword="true"/> if the dictionaries contain the same keys and values,
        /// <see langword="false"/> otherwise.
        /// </returns>
        public bool RecursiveEqual(Dictionary<TKey, TValue> other)
        {
            return _underlyingDict.RecursiveEqual(other._underlyingDict);
        }

        // IDictionary<TKey, TValue>

        /// <summary>
        /// Returns the value at the given <paramref name="key"/>.
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// The property is assigned and the dictionary is read-only.
        /// </exception>
        /// <exception cref="KeyNotFoundException">
        /// The property is retrieved and an entry for <paramref name="key"/>
        /// does not exist in the dictionary.
        /// </exception>
        /// <value>The value at the given <paramref name="key"/>.</value>
        public TValue this[TKey key]
        {
            get
            {
                using var variantKey = VariantUtils.CreateFrom(key);
                var self = (godot_dictionary)_underlyingDict.NativeValue;

                if (NativeFuncs.godotsharp_dictionary_try_get_value(ref self,
                        variantKey, out godot_variant value).ToBool())
                {
                    using (value)
                        return VariantUtils.ConvertTo<TValue>(value);
                }
                else
                {
                    throw new KeyNotFoundException();
                }
            }
            set
            {
                ThrowIfReadOnly();

                using var variantKey = VariantUtils.CreateFrom(key);
                using var variantValue = VariantUtils.CreateFrom(value);
                var self = (godot_dictionary)_underlyingDict.NativeValue;
                NativeFuncs.godotsharp_dictionary_set_value(ref self,
                    variantKey, variantValue);
            }
        }

        /// <summary>
        /// Gets the collection of keys in this <see cref="Dictionary{TKey, TValue}"/>.
        /// </summary>
        public ICollection<TKey> Keys
        {
            get
            {
                godot_array keyArray;
                var self = (godot_dictionary)_underlyingDict.NativeValue;
                NativeFuncs.godotsharp_dictionary_keys(ref self, out keyArray);
                return Array<TKey>.CreateTakingOwnershipOfDisposableValue(keyArray);
            }
        }

        /// <summary>
        /// Gets the collection of elements in this <see cref="Dictionary{TKey, TValue}"/>.
        /// </summary>
        public ICollection<TValue> Values
        {
            get
            {
                godot_array valuesArray;
                var self = (godot_dictionary)_underlyingDict.NativeValue;
                NativeFuncs.godotsharp_dictionary_values(ref self, out valuesArray);
                return Array<TValue>.CreateTakingOwnershipOfDisposableValue(valuesArray);
            }
        }

        IEnumerable<TKey> IReadOnlyDictionary<TKey, TValue>.Keys => Keys;

        IEnumerable<TValue> IReadOnlyDictionary<TKey, TValue>.Values => Values;

        private KeyValuePair<TKey, TValue> GetKeyValuePair(int index)
        {
            var self = (godot_dictionary)_underlyingDict.NativeValue;
            NativeFuncs.godotsharp_dictionary_key_value_pair_at(ref self, index,
                out godot_variant key,
                out godot_variant value);
            using (key)
            using (value)
            {
                return new KeyValuePair<TKey, TValue>(
                    VariantUtils.ConvertTo<TKey>(key),
                    VariantUtils.ConvertTo<TValue>(value));
            }
        }

        /// <summary>
        /// Adds an object <paramref name="value"/> at key <paramref name="key"/>
        /// to this <see cref="Dictionary{TKey, TValue}"/>.
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// The dictionary is read-only.
        /// </exception>
        /// <exception cref="ArgumentException">
        /// An element with the same <paramref name="key"/> already exists.
        /// </exception>
        /// <param name="key">The key at which to add the object.</param>
        /// <param name="value">The object to add.</param>
        public void Add(TKey key, TValue value)
        {
            ThrowIfReadOnly();

            using var variantKey = VariantUtils.CreateFrom(key);
            var self = (godot_dictionary)_underlyingDict.NativeValue;

            if (NativeFuncs.godotsharp_dictionary_contains_key(ref self, variantKey).ToBool())
                throw new ArgumentException("An element with the same key already exists.", nameof(key));

            using var variantValue = VariantUtils.CreateFrom(value);
            NativeFuncs.godotsharp_dictionary_add(ref self, variantKey, variantValue);
        }

        /// <summary>
        /// Checks if this <see cref="Dictionary{TKey, TValue}"/> contains the given key.
        /// </summary>
        /// <param name="key">The key to look for.</param>
        /// <returns>Whether or not this dictionary contains the given key.</returns>
        public bool ContainsKey(TKey key)
        {
            using var variantKey = VariantUtils.CreateFrom(key);
            var self = (godot_dictionary)_underlyingDict.NativeValue;
            return NativeFuncs.godotsharp_dictionary_contains_key(ref self, variantKey).ToBool();
        }

        /// <summary>
        /// Removes an element from this <see cref="Dictionary{TKey, TValue}"/> by key.
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// The dictionary is read-only.
        /// </exception>
        /// <param name="key">The key of the element to remove.</param>
        public bool Remove(TKey key)
        {
            ThrowIfReadOnly();

            using var variantKey = VariantUtils.CreateFrom(key);
            var self = (godot_dictionary)_underlyingDict.NativeValue;
            return NativeFuncs.godotsharp_dictionary_remove_key(ref self, variantKey).ToBool();
        }

        /// <summary>
        /// Gets the value for the given <paramref name="key"/> in the dictionary.
        /// Returns <see langword="true"/> if an entry for the given key exists in
        /// the dictionary; otherwise, returns <see langword="false"/>.
        /// </summary>
        /// <param name="key">The key of the element to get.</param>
        /// <param name="value">The value at the given <paramref name="key"/>.</param>
        /// <returns>If an entry was found for the given <paramref name="key"/>.</returns>
        public bool TryGetValue(TKey key, [MaybeNullWhen(false)] out TValue value)
        {
            using var variantKey = VariantUtils.CreateFrom(key);
            var self = (godot_dictionary)_underlyingDict.NativeValue;
            bool found = NativeFuncs.godotsharp_dictionary_try_get_value(ref self,
                variantKey, out godot_variant retValue).ToBool();

            using (retValue)
                value = found ? VariantUtils.ConvertTo<TValue>(retValue) : default;

            return found;
        }

        // ICollection<KeyValuePair<TKey, TValue>>

        /// <summary>
        /// Returns the number of elements in this <see cref="Dictionary{TKey, TValue}"/>.
        /// This is also known as the size or length of the dictionary.
        /// </summary>
        /// <returns>The number of elements.</returns>
        public int Count => _underlyingDict.Count;

        /// <summary>
        /// Returns <see langword="true"/> if the dictionary is read-only.
        /// See <see cref="MakeReadOnly"/>.
        /// </summary>
        public bool IsReadOnly => _underlyingDict.IsReadOnly;

        /// <summary>
        /// Makes the <see cref="Dictionary{TKey, TValue}"/> read-only, i.e. disabled
        /// modying of the dictionary's elements. Does not apply to nested content,
        /// e.g. content of nested dictionaries.
        /// </summary>
        public void MakeReadOnly()
        {
            _underlyingDict.MakeReadOnly();
        }

        void ICollection<KeyValuePair<TKey, TValue>>.Add(KeyValuePair<TKey, TValue> item)
            => Add(item.Key, item.Value);

        /// <summary>
        /// Clears the dictionary, removing all entries from it.
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// The dictionary is read-only.
        /// </exception>
        public void Clear() => _underlyingDict.Clear();

        bool ICollection<KeyValuePair<TKey, TValue>>.Contains(KeyValuePair<TKey, TValue> item)
        {
            using var variantKey = VariantUtils.CreateFrom(item.Key);
            var self = (godot_dictionary)_underlyingDict.NativeValue;
            bool found = NativeFuncs.godotsharp_dictionary_try_get_value(ref self,
                variantKey, out godot_variant retValue).ToBool();

            using (retValue)
            {
                if (!found)
                    return false;

                using var variantValue = VariantUtils.CreateFrom(item.Value);
                return NativeFuncs.godotsharp_variant_equals(variantValue, retValue).ToBool();
            }
        }

        /// <summary>
        /// Copies the elements of this <see cref="Dictionary{TKey, TValue}"/> to the given
        /// untyped C# array, starting at the given index.
        /// </summary>
        /// <exception cref="ArgumentNullException">
        /// The <paramref name="array"/> is <see langword="null"/>.
        /// </exception>
        /// <exception cref="ArgumentOutOfRangeException">
        /// <paramref name="arrayIndex"/> is less than 0 or greater than the array's size.
        /// </exception>
        /// <exception cref="ArgumentException">
        /// The destination array was not long enough.
        /// </exception>
        /// <param name="array">The array to copy to.</param>
        /// <param name="arrayIndex">The index to start at.</param>
        void ICollection<KeyValuePair<TKey, TValue>>.CopyTo(KeyValuePair<TKey, TValue>[] array, int arrayIndex)
        {
            if (array == null)
                throw new ArgumentNullException(nameof(array), "Value cannot be null.");

            if (arrayIndex < 0)
                throw new ArgumentOutOfRangeException(nameof(arrayIndex),
                    "Number was less than the array's lower bound in the first dimension.");

            int count = Count;

            if (array.Length < (arrayIndex + count))
                throw new ArgumentException(
                    "Destination array was not long enough. Check destIndex and length, and the array's lower bounds.");

            for (int i = 0; i < count; i++)
            {
                array[arrayIndex] = GetKeyValuePair(i);
                arrayIndex++;
            }
        }

        bool ICollection<KeyValuePair<TKey, TValue>>.Remove(KeyValuePair<TKey, TValue> item)
        {
            ThrowIfReadOnly();

            using var variantKey = VariantUtils.CreateFrom(item.Key);
            var self = (godot_dictionary)_underlyingDict.NativeValue;
            bool found = NativeFuncs.godotsharp_dictionary_try_get_value(ref self,
                variantKey, out godot_variant retValue).ToBool();

            using (retValue)
            {
                if (!found)
                    return false;

                using var variantValue = VariantUtils.CreateFrom(item.Value);
                if (NativeFuncs.godotsharp_variant_equals(variantValue, retValue).ToBool())
                {
                    return NativeFuncs.godotsharp_dictionary_remove_key(
                        ref self, variantKey).ToBool();
                }

                return false;
            }
        }

        // IEnumerable<KeyValuePair<TKey, TValue>>

        /// <summary>
        /// Gets an enumerator for this <see cref="Dictionary{TKey, TValue}"/>.
        /// </summary>
        /// <returns>An enumerator.</returns>
        public IEnumerator<KeyValuePair<TKey, TValue>> GetEnumerator()
        {
            for (int i = 0; i < Count; i++)
            {
                yield return GetKeyValuePair(i);
            }
        }

        IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();

        /// <summary>
        /// Converts this <see cref="Dictionary{TKey, TValue}"/> to a string.
        /// </summary>
        /// <returns>A string representation of this dictionary.</returns>
        public override string ToString() => _underlyingDict.ToString();

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static implicit operator Variant(Dictionary<TKey, TValue> from) => Variant.CreateFrom(from);

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static explicit operator Dictionary<TKey, TValue>(Variant from) =>
            from.AsGodotDictionary<TKey, TValue>();

        private void ThrowIfReadOnly()
        {
            if (IsReadOnly)
            {
                throw new InvalidOperationException("Dictionary instance is read-only.");
            }
        }
    }
}
