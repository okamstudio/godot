using System;
using System.Collections.Generic;
using System.Collections;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Runtime.CompilerServices;
using Godot.NativeInterop;
using System.Diagnostics;

#nullable enable

namespace Godot.Collections
{
    /// <summary>
    /// Wrapper around Godot's Array class, an array of Variant
    /// typed elements allocated in the engine in C++. Useful when
    /// interfacing with the engine. Otherwise prefer .NET collections
    /// such as <see cref="System.Array"/> or <see cref="List{T}"/>.
    /// </summary>
    [DebuggerTypeProxy(typeof(ArrayDebugView<Variant>))]
    [DebuggerDisplay("Count = {Count}")]
#pragma warning disable CA1710 // Identifiers should have correct suffix
    public sealed class Array :
#pragma warning restore CA1710
        IList<Variant>,
        IReadOnlyList<Variant>,
        ICollection,
        IDisposable
    {
        internal godot_array.movable NativeValue;

        private WeakReference<IDisposable>? _weakReferenceToSelf;

        /// <summary>
        /// Constructs a new empty <see cref="Array"/>.
        /// </summary>
        public Array()
        {
            NativeValue = (godot_array.movable)NativeFuncs.godotsharp_array_new();
            _weakReferenceToSelf = DisposablesTracker.RegisterDisposable(this);
        }

        /// <summary>
        /// Constructs a new <see cref="Array"/> from the given collection's elements.
        /// </summary>
        /// <exception cref="ArgumentNullException">
        /// The <paramref name="collection"/> is <see langword="null"/>.
        /// </exception>
        /// <param name="collection">The collection of elements to construct from.</param>
        /// <returns>A new Godot Array.</returns>
        public Array(IEnumerable<Variant> collection) : this()
        {
            ArgumentNullException.ThrowIfNull(collection);

            foreach (Variant element in collection)
                Add(element);
        }

        /// <summary>
        /// Constructs a new <see cref="Array"/> from the given objects.
        /// </summary>
        /// <exception cref="ArgumentNullException">
        /// The <paramref name="array"/> is <see langword="null"/>.
        /// </exception>
        /// <param name="array">The objects to put in the new array.</param>
        /// <returns>A new Godot Array.</returns>
        public Array(Variant[] array)
        {
            ArgumentNullException.ThrowIfNull(array);

            NativeValue = (godot_array.movable)NativeFuncs.godotsharp_array_new();
            _weakReferenceToSelf = DisposablesTracker.RegisterDisposable(this);

            int length = array.Length;

            Resize(length);

            for (int i = 0; i < length; i++)
                this[i] = array[i];
        }

        /// <summary>
        /// Constructs a new <see cref="Array"/> from the given span's elements.
        /// </summary>
        /// <exception cref="ArgumentNullException">
        /// The <paramref name="array"/> is <see langword="null"/>.
        /// </exception>
        /// <returns>A new Godot Array.</returns>
        public Array(Span<StringName> array)
        {
            if (array == null)
                throw new ArgumentNullException(nameof(array));

            NativeValue = (godot_array.movable)NativeFuncs.godotsharp_array_new();
            _weakReferenceToSelf = DisposablesTracker.RegisterDisposable(this);

            int length = array.Length;

            Resize(length);

            for (int i = 0; i < length; i++)
                this[i] = array[i];
        }

        /// <summary>
        /// Constructs a new <see cref="Array"/> from the given span's elements.
        /// </summary>
        /// <exception cref="ArgumentNullException">
        /// The <paramref name="array"/> is <see langword="null"/>.
        /// </exception>
        /// <returns>A new Godot Array.</returns>
        public Array(Span<NodePath> array)
        {
            if (array == null)
                throw new ArgumentNullException(nameof(array));

            NativeValue = (godot_array.movable)NativeFuncs.godotsharp_array_new();
            _weakReferenceToSelf = DisposablesTracker.RegisterDisposable(this);

            int length = array.Length;

            Resize(length);

            for (int i = 0; i < length; i++)
                this[i] = array[i];
        }

        /// <summary>
        /// Constructs a new <see cref="Array"/> from the given span's elements.
        /// </summary>
        /// <exception cref="ArgumentNullException">
        /// The <paramref name="array"/> is <see langword="null"/>.
        /// </exception>
        /// <returns>A new Godot Array.</returns>
        public Array(Span<Rid> array)
        {
            if (array == null)
                throw new ArgumentNullException(nameof(array));

            NativeValue = (godot_array.movable)NativeFuncs.godotsharp_array_new();
            _weakReferenceToSelf = DisposablesTracker.RegisterDisposable(this);

            int length = array.Length;

            Resize(length);

            for (int i = 0; i < length; i++)
                this[i] = array[i];
        }

        // We must use ReadOnlySpan instead of Span here as this can accept implicit conversions
        // from derived types (e.g.: Node[]). Implicit conversion from Derived[] to Base[] are
        // fine as long as the array is not mutated. However, Span does this type checking at
        // instantiation, so it's not possible to use it even when not mutating anything.
        /// <summary>
        /// Constructs a new <see cref="Array"/> from the given ReadOnlySpan's elements.
        /// </summary>
        /// <exception cref="ArgumentNullException">
        /// The <paramref name="array"/> is <see langword="null"/>.
        /// </exception>
        /// <returns>A new Godot Array.</returns>
        public Array(ReadOnlySpan<GodotObject> array)
        {
            if (array == null)
                throw new ArgumentNullException(nameof(array));

            NativeValue = (godot_array.movable)NativeFuncs.godotsharp_array_new();
            _weakReferenceToSelf = DisposablesTracker.RegisterDisposable(this);

            int length = array.Length;

            Resize(length);

            for (int i = 0; i < length; i++)
                this[i] = array[i];
        }

        private Array(godot_array nativeValueToOwn)
        {
            NativeValue = (godot_array.movable)(nativeValueToOwn.IsAllocated ?
                nativeValueToOwn :
                NativeFuncs.godotsharp_array_new());
            _weakReferenceToSelf = DisposablesTracker.RegisterDisposable(this);
        }

        // Explicit name to make it very clear
        internal static Array CreateTakingOwnershipOfDisposableValue(godot_array nativeValueToOwn)
            => new Array(nativeValueToOwn);

        ~Array()
        {
            Dispose(false);
        }

        /// <summary>
        /// Disposes of this <see cref="Array"/>.
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
        /// Returns a copy of the <see cref="Array"/>.
        /// If <paramref name="deep"/> is <see langword="true"/>, a deep copy if performed:
        /// all nested arrays and dictionaries are duplicated and will not be shared with
        /// the original array. If <see langword="false"/>, a shallow copy is made and
        /// references to the original nested arrays and dictionaries are kept, so that
        /// modifying a sub-array or dictionary in the copy will also impact those
        /// referenced in the source array. Note that any <see cref="GodotObject"/> derived
        /// elements will be shallow copied regardless of the <paramref name="deep"/>
        /// setting.
        /// </summary>
        /// <param name="deep">If <see langword="true"/>, performs a deep copy.</param>
        /// <returns>A new Godot Array.</returns>
        public Array Duplicate(bool deep = false)
        {
            godot_array newArray;
            var self = (godot_array)NativeValue;
            NativeFuncs.godotsharp_array_duplicate(ref self, deep.ToGodotBool(), out newArray);
            return CreateTakingOwnershipOfDisposableValue(newArray);
        }

        /// <summary>
        /// Assigns the given value to all elements in the array. This can typically be
        /// used together with <see cref="Resize(int)"/> to create an array with a given
        /// size and initialized elements.
        /// Note: If <paramref name="value"/> is of a reference type (<see cref="GodotObject"/>
        /// derived, <see cref="Array"/> or <see cref="Dictionary"/>, etc.) then the array
        /// is filled with the references to the same object, i.e. no duplicates are
        /// created.
        /// </summary>
        /// <example>
        /// <code>
        /// var array = new Godot.Collections.Array();
        /// array.Resize(10);
        /// array.Fill(0); // Initialize the 10 elements to 0.
        /// </code>
        /// </example>
        /// <exception cref="InvalidOperationException">
        /// The array is read-only.
        /// </exception>
        /// <param name="value">The value to fill the array with.</param>
        public void Fill(Variant value)
        {
            ThrowIfReadOnly();

            godot_variant variantValue = (godot_variant)value.NativeVar;
            var self = (godot_array)NativeValue;
            NativeFuncs.godotsharp_array_fill(ref self, variantValue);
        }

        /// <summary>
        /// Returns the maximum value contained in the array if all elements are of
        /// comparable types. If the elements can't be compared, <see langword="null"/>
        /// is returned.
        /// </summary>
        /// <returns>The maximum value contained in the array.</returns>
        public Variant Max()
        {
            godot_variant resVariant;
            var self = (godot_array)NativeValue;
            NativeFuncs.godotsharp_array_max(ref self, out resVariant);
            return Variant.CreateTakingOwnershipOfDisposableValue(resVariant);
        }

        /// <summary>
        /// Returns the minimum value contained in the array if all elements are of
        /// comparable types. If the elements can't be compared, <see langword="null"/>
        /// is returned.
        /// </summary>
        /// <returns>The minimum value contained in the array.</returns>
        public Variant Min()
        {
            godot_variant resVariant;
            var self = (godot_array)NativeValue;
            NativeFuncs.godotsharp_array_min(ref self, out resVariant);
            return Variant.CreateTakingOwnershipOfDisposableValue(resVariant);
        }

        /// <summary>
        /// Returns a random value from the target array.
        /// </summary>
        /// <example>
        /// <code>
        /// var array = new Godot.Collections.Array { 1, 2, 3, 4 };
        /// GD.Print(array.PickRandom()); // Prints either of the four numbers.
        /// </code>
        /// </example>
        /// <returns>A random element from the array.</returns>
        public Variant PickRandom()
        {
            godot_variant resVariant;
            var self = (godot_array)NativeValue;
            NativeFuncs.godotsharp_array_pick_random(ref self, out resVariant);
            return Variant.CreateTakingOwnershipOfDisposableValue(resVariant);
        }

        /// <summary>
        /// Compares this <see cref="Array"/> against the <paramref name="other"/>
        /// <see cref="Array"/> recursively. Returns <see langword="true"/> if the
        /// sizes and contents of the arrays are equal, <see langword="false"/>
        /// otherwise.
        /// </summary>
        /// <param name="other">The other array to compare against.</param>
        /// <returns>
        /// <see langword="true"/> if the sizes and contents of the arrays are equal,
        /// <see langword="false"/> otherwise.
        /// </returns>
        public bool RecursiveEqual(Array other)
        {
            var self = (godot_array)NativeValue;
            var otherVariant = (godot_array)other.NativeValue;
            return NativeFuncs.godotsharp_array_recursive_equal(ref self, otherVariant).ToBool();
        }

        /// <summary>
        /// Resizes the array to contain a different number of elements. If the array
        /// size is smaller, elements are cleared, if bigger, new elements are
        /// <see langword="null"/>.
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// The array is read-only.
        /// </exception>
        /// <param name="newSize">The new size of the array.</param>
        /// <returns><see cref="Error.Ok"/> if successful, or an error code.</returns>
        public Error Resize(int newSize)
        {
            ThrowIfReadOnly();

            var self = (godot_array)NativeValue;
            return NativeFuncs.godotsharp_array_resize(ref self, newSize);
        }

        /// <summary>
        /// Reverses the order of the elements in the array.
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// The array is read-only.
        /// </exception>
        public void Reverse()
        {
            ThrowIfReadOnly();

            var self = (godot_array)NativeValue;
            NativeFuncs.godotsharp_array_reverse(ref self);
        }

        /// <summary>
        /// Shuffles the array such that the items will have a random order.
        /// This method uses the global random number generator common to methods
        /// such as <see cref="GD.Randi"/>. Call <see cref="GD.Randomize"/> to
        /// ensure that a new seed will be used each time if you want
        /// non-reproducible shuffling.
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// The array is read-only.
        /// </exception>
        public void Shuffle()
        {
            ThrowIfReadOnly();

            var self = (godot_array)NativeValue;
            NativeFuncs.godotsharp_array_shuffle(ref self);
        }

        /// <summary>
        /// Creates a shallow copy of a range of elements in the source <see cref="Array"/>.
        /// </summary>
        /// <exception cref="ArgumentOutOfRangeException">
        /// <paramref name="start"/> is less than 0 or greater than the array's size.
        /// </exception>
        /// <param name="start">The zero-based index at which the range starts.</param>
        /// <returns>A new array that contains the elements inside the slice range.</returns>
        public Array Slice(int start)
        {
            if (start < 0 || start > Count)
                throw new ArgumentOutOfRangeException(nameof(start));

            return GetSliceRange(start, Count, step: 1, deep: false);
        }

        /// <summary>
        /// Creates a shallow copy of a range of elements in the source <see cref="Array"/>.
        /// </summary>
        /// <exception cref="ArgumentOutOfRangeException">
        /// <paramref name="start"/> is less than 0 or greater than the array's size.
        /// -or-
        /// <paramref name="length"/> is less than 0 or greater than the array's size.
        /// </exception>
        /// <param name="start">The zero-based index at which the range starts.</param>
        /// <param name="length">The length of the range.</param>
        /// <returns>A new array that contains the elements inside the slice range.</returns>
        // The Slice method must have this signature to get implicit Range support.
        public Array Slice(int start, int length)
        {
            if (start < 0 || start > Count)
                throw new ArgumentOutOfRangeException(nameof(start));

            if (length < 0 || length > Count)
                throw new ArgumentOutOfRangeException(nameof(start));

            return GetSliceRange(start, start + length, step: 1, deep: false);
        }

        /// <summary>
        /// Returns the slice of the <see cref="Array"/>, from <paramref name="start"/>
        /// (inclusive) to <paramref name="end"/> (exclusive), as a new <see cref="Array"/>.
        /// The absolute value of <paramref name="start"/> and <paramref name="end"/>
        /// will be clamped to the array size.
        /// If either <paramref name="start"/> or <paramref name="end"/> are negative, they
        /// will be relative to the end of the array (i.e. <c>arr.GetSliceRange(0, -2)</c>
        /// is a shorthand for <c>arr.GetSliceRange(0, arr.Count - 2)</c>).
        /// If specified, <paramref name="step"/> is the relative index between source
        /// elements. It can be negative, then <paramref name="start"/> must be higher than
        /// <paramref name="end"/>. For example, <c>[0, 1, 2, 3, 4, 5].GetSliceRange(5, 1, -2)</c>
        /// returns <c>[5, 3]</c>.
        /// If <paramref name="deep"/> is true, each element will be copied by value
        /// rather than by reference.
        /// </summary>
        /// <param name="start">The zero-based index at which the range starts.</param>
        /// <param name="end">The zero-based index at which the range ends.</param>
        /// <param name="step">The relative index between source elements to take.</param>
        /// <param name="deep">If <see langword="true"/>, performs a deep copy.</param>
        /// <returns>A new array that contains the elements inside the slice range.</returns>
        public Array GetSliceRange(int start, int end, int step = 1, bool deep = false)
        {
            godot_array newArray;
            var self = (godot_array)NativeValue;
            NativeFuncs.godotsharp_array_slice(ref self, start, end, step, deep.ToGodotBool(), out newArray);
            return CreateTakingOwnershipOfDisposableValue(newArray);
        }

        /// <summary>
        /// Sorts the array.
        /// Note: The sorting algorithm used is not stable. This means that values
        /// considered equal may have their order changed when using <see cref="Sort"/>.
        /// Note: Strings are sorted in alphabetical order (as opposed to natural order).
        /// This may lead to unexpected behavior when sorting an array of strings ending
        /// with a sequence of numbers.
        /// To sort with a custom predicate use
        /// <see cref="Enumerable.OrderBy{TSource, TKey}(IEnumerable{TSource}, Func{TSource, TKey})"/>.
        /// </summary>
        /// <example>
        /// <code>
        /// var strings = new Godot.Collections.Array { "string1", "string2", "string10", "string11" };
        /// strings.Sort();
        /// GD.Print(strings); // Prints [string1, string10, string11, string2]
        /// </code>
        /// </example>
        /// <exception cref="InvalidOperationException">
        /// The array is read-only.
        /// </exception>
        public void Sort()
        {
            ThrowIfReadOnly();

            var self = (godot_array)NativeValue;
            NativeFuncs.godotsharp_array_sort(ref self);
        }

        /// <summary>
        /// Concatenates two <see cref="Array"/>s together, with the <paramref name="right"/>
        /// being added to the end of the <see cref="Array"/> specified in <paramref name="left"/>.
        /// For example, <c>[1, 2] + [3, 4]</c> results in <c>[1, 2, 3, 4]</c>.
        /// </summary>
        /// <param name="left">The first array.</param>
        /// <param name="right">The second array.</param>
        /// <returns>A new Godot Array with the contents of both arrays.</returns>
        public static Array operator +(Array left, Array right)
        {
            if (left == null)
            {
                if (right == null)
                    return new Array();

                return right.Duplicate(deep: false);
            }

            if (right == null)
                return left.Duplicate(deep: false);

            int leftCount = left.Count;
            int rightCount = right.Count;

            Array newArray = left.Duplicate(deep: false);
            newArray.Resize(leftCount + rightCount);

            for (int i = 0; i < rightCount; i++)
                newArray[i + leftCount] = right[i];

            return newArray;
        }

        /// <summary>
        /// Returns the item at the given <paramref name="index"/>.
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// The property is assigned and the array is read-only.
        /// </exception>
        /// <exception cref="ArgumentOutOfRangeException">
        /// <paramref name="index"/> is less than 0 or greater than the array's size.
        /// </exception>
        /// <value>The <see cref="Variant"/> item at the given <paramref name="index"/>.</value>
        public unsafe Variant this[int index]
        {
            get
            {
                GetVariantBorrowElementAt(index, out godot_variant borrowElem);
                return Variant.CreateCopyingBorrowed(borrowElem);
            }
            set
            {
                ThrowIfReadOnly();

                if (index < 0 || index >= Count)
                    throw new ArgumentOutOfRangeException(nameof(index));

                var self = (godot_array)NativeValue;
                godot_variant* ptrw = NativeFuncs.godotsharp_array_ptrw(ref self);
                godot_variant* itemPtr = &ptrw[index];
                (*itemPtr).Dispose();
                *itemPtr = value.CopyNativeVariant();
            }
        }

        /// <summary>
        /// Adds an item to the end of this <see cref="Array"/>.
        /// This is the same as <c>append</c> or <c>push_back</c> in GDScript.
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// The array is read-only.
        /// </exception>
        /// <param name="item">The <see cref="Variant"/> item to add.</param>
        public void Add(Variant item)
        {
            ThrowIfReadOnly();

            godot_variant variantValue = (godot_variant)item.NativeVar;
            var self = (godot_array)NativeValue;
            _ = NativeFuncs.godotsharp_array_add(ref self, variantValue);
        }

        /// <summary>
        /// Adds the elements of the specified collection to the end of this <see cref="Array"/>.
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// The array is read-only.
        /// </exception>
        /// <exception cref="ArgumentNullException">
        /// The <paramref name="collection"/> is <see langword="null"/>.
        /// </exception>
        /// <param name="collection">Collection of <see cref="Variant"/> items to add.</param>
        public void AddRange<[MustBeVariant] T>(IEnumerable<T> collection)
        {
            ThrowIfReadOnly();

            if (collection == null)
                throw new ArgumentNullException(nameof(collection), "Value cannot be null.");

            // If the collection is another Godot Array, we can add the items
            // with a single interop call.
            if (collection is Array array)
            {
                var self = (godot_array)NativeValue;
                var collectionNative = (godot_array)array.NativeValue;
                _ = NativeFuncs.godotsharp_array_add_range(ref self, collectionNative);
                return;
            }
            if (collection is Array<T> typedArray)
            {
                var self = (godot_array)NativeValue;
                var collectionNative = (godot_array)typedArray.NativeValue;
                _ = NativeFuncs.godotsharp_array_add_range(ref self, collectionNative);
                return;
            }

            // If we can retrieve the count of the collection without enumerating it
            // (e.g.: the collections is a List<T>), use it to resize the array once
            // instead of growing it as we add items.
            if (collection.TryGetNonEnumeratedCount(out int count))
            {
                int oldCount = Count;
                Resize(Count + count);

                using var enumerator = collection.GetEnumerator();

                for (int i = 0; i < count; i++)
                {
                    enumerator.MoveNext();
                    this[oldCount + i] = Variant.From(enumerator.Current);
                }

                return;
            }

            foreach (var item in collection)
            {
                Add(Variant.From(item));
            }
        }

        /// <summary>
        /// Finds the index of an existing value using binary search.
        /// If the value is not present in the array, it returns the bitwise
        /// complement of the insertion index that maintains sorting order.
        /// Note: Calling <see cref="BinarySearch(int, int, Variant)"/> on an
        /// unsorted array results in unexpected behavior.
        /// </summary>
        /// <exception cref="ArgumentOutOfRangeException">
        /// <paramref name="index"/> is less than 0.
        /// -or-
        /// <paramref name="count"/> is less than 0.
        /// </exception>
        /// <exception cref="ArgumentException">
        /// <paramref name="index"/> and <paramref name="count"/> do not denote
        /// a valid range in the <see cref="Array"/>.
        /// </exception>
        /// <param name="index">The starting index of the range to search.</param>
        /// <param name="count">The length of the range to search.</param>
        /// <param name="item">The object to locate.</param>
        /// <returns>
        /// The index of the item in the array, if <paramref name="item"/> is found;
        /// otherwise, a negative number that is the bitwise complement of the index
        /// of the next element that is larger than <paramref name="item"/> or, if
        /// there is no larger element, the bitwise complement of <see cref="Count"/>.
        /// </returns>
        public int BinarySearch(int index, int count, Variant item)
        {
            if (index < 0)
                throw new ArgumentOutOfRangeException(nameof(index), "index cannot be negative.");
            if (count < 0)
                throw new ArgumentOutOfRangeException(nameof(count), "count cannot be negative.");
            if (Count - index < count)
                throw new ArgumentException("length is out of bounds or count is greater than the number of elements.");

            if (Count == 0)
            {
                // Special case for empty array to avoid an interop call.
                return -1;
            }

            godot_variant variantValue = (godot_variant)item.NativeVar;
            var self = (godot_array)NativeValue;
            return NativeFuncs.godotsharp_array_binary_search(ref self, index, count, variantValue);
        }

        /// <summary>
        /// Finds the index of an existing value using binary search.
        /// If the value is not present in the array, it returns the bitwise
        /// complement of the insertion index that maintains sorting order.
        /// Note: Calling <see cref="BinarySearch(Variant)"/> on an unsorted
        /// array results in unexpected behavior.
        /// </summary>
        /// <param name="item">The object to locate.</param>
        /// <returns>
        /// The index of the item in the array, if <paramref name="item"/> is found;
        /// otherwise, a negative number that is the bitwise complement of the index
        /// of the next element that is larger than <paramref name="item"/> or, if
        /// there is no larger element, the bitwise complement of <see cref="Count"/>.
        /// </returns>
        public int BinarySearch(Variant item)
        {
            return BinarySearch(0, Count, item);
        }

        /// <summary>
        /// Returns <see langword="true"/> if the array contains the given value.
        /// </summary>
        /// <example>
        /// <code>
        /// var arr = new Godot.Collections.Array { "inside", 7 };
        /// GD.Print(arr.Contains("inside")); // True
        /// GD.Print(arr.Contains("outside")); // False
        /// GD.Print(arr.Contains(7)); // True
        /// GD.Print(arr.Contains("7")); // False
        /// </code>
        /// </example>
        /// <param name="item">The <see cref="Variant"/> item to look for.</param>
        /// <returns>Whether or not this array contains the given item.</returns>
        public bool Contains(Variant item) => IndexOf(item) != -1;

        /// <summary>
        /// Clears the array. This is the equivalent to using <see cref="Resize(int)"/>
        /// with a size of <c>0</c>
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// The array is read-only.
        /// </exception>
        public void Clear() => Resize(0);

        /// <summary>
        /// Searches the array for a value and returns its index or <c>-1</c> if not found.
        /// </summary>
        /// <param name="item">The <see cref="Variant"/> item to search for.</param>
        /// <returns>The index of the item, or -1 if not found.</returns>
        public int IndexOf(Variant item)
        {
            if (Count == 0)
            {
                // Special case for empty array to avoid an interop call.
                return -1;
            }

            godot_variant variantValue = (godot_variant)item.NativeVar;
            var self = (godot_array)NativeValue;
            return NativeFuncs.godotsharp_array_index_of(ref self, variantValue);
        }

        /// <summary>
        /// Searches the array for a value and returns its index or <c>-1</c> if not found.
        /// </summary>
        /// <exception cref="ArgumentOutOfRangeException">
        /// <paramref name="index"/> is less than 0 or greater than the array's size.
        /// </exception>
        /// <param name="item">The <see cref="Variant"/> item to search for.</param>
        /// <param name="index">The initial search index to start from.</param>
        /// <returns>The index of the item, or -1 if not found.</returns>
        public int IndexOf(Variant item, int index)
        {
            if (index < 0 || index > Count)
                throw new ArgumentOutOfRangeException(nameof(index));

            if (Count == 0)
            {
                // Special case for empty array to avoid an interop call.
                return -1;
            }

            godot_variant variantValue = (godot_variant)item.NativeVar;
            var self = (godot_array)NativeValue;
            return NativeFuncs.godotsharp_array_index_of(ref self, variantValue, index);
        }

        /// <summary>
        /// Searches the array for a value in reverse order and returns its index
        /// or <c>-1</c> if not found.
        /// </summary>
        /// <param name="item">The <see cref="Variant"/> item to search for.</param>
        /// <returns>The index of the item, or -1 if not found.</returns>
        public int LastIndexOf(Variant item)
        {
            if (Count == 0)
            {
                // Special case for empty array to avoid an interop call.
                return -1;
            }

            godot_variant variantValue = (godot_variant)item.NativeVar;
            var self = (godot_array)NativeValue;
            return NativeFuncs.godotsharp_array_last_index_of(ref self, variantValue, Count - 1);
        }

        /// <summary>
        /// Searches the array for a value in reverse order and returns its index
        /// or <c>-1</c> if not found.
        /// </summary>
        /// <exception cref="ArgumentOutOfRangeException">
        /// <paramref name="index"/> is less than 0 or greater than the array's size.
        /// </exception>
        /// <param name="item">The <see cref="Variant"/> item to search for.</param>
        /// <param name="index">The initial search index to start from.</param>
        /// <returns>The index of the item, or -1 if not found.</returns>
        public int LastIndexOf(Variant item, int index)
        {
            if (index < 0 || index >= Count)
                throw new ArgumentOutOfRangeException(nameof(index));

            if (Count == 0)
            {
                // Special case for empty array to avoid an interop call.
                return -1;
            }

            godot_variant variantValue = (godot_variant)item.NativeVar;
            var self = (godot_array)NativeValue;
            return NativeFuncs.godotsharp_array_last_index_of(ref self, variantValue, index);
        }

        /// <summary>
        /// Inserts a new element at a given position in the array. The position
        /// must be valid, or at the end of the array (<c>pos == Count - 1</c>).
        /// Existing items will be moved to the right.
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// The array is read-only.
        /// </exception>
        /// <exception cref="ArgumentOutOfRangeException">
        /// <paramref name="index"/> is less than 0 or greater than the array's size.
        /// </exception>
        /// <param name="index">The index to insert at.</param>
        /// <param name="item">The <see cref="Variant"/> item to insert.</param>
        public void Insert(int index, Variant item)
        {
            ThrowIfReadOnly();

            if (index < 0 || index > Count)
                throw new ArgumentOutOfRangeException(nameof(index));

            godot_variant variantValue = (godot_variant)item.NativeVar;
            var self = (godot_array)NativeValue;
            NativeFuncs.godotsharp_array_insert(ref self, index, variantValue);
        }

        /// <summary>
        /// Removes the first occurrence of the specified <paramref name="item"/>
        /// from this <see cref="Array"/>.
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// The array is read-only.
        /// </exception>
        /// <param name="item">The value to remove.</param>
        public bool Remove(Variant item)
        {
            ThrowIfReadOnly();

            int index = IndexOf(item);
            if (index >= 0)
            {
                RemoveAt(index);
                return true;
            }

            return false;
        }

        /// <summary>
        /// Removes an element from the array by index.
        /// To remove an element by searching for its value, use
        /// <see cref="Remove(Variant)"/> instead.
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// The array is read-only.
        /// </exception>
        /// <exception cref="ArgumentOutOfRangeException">
        /// <paramref name="index"/> is less than 0 or greater than the array's size.
        /// </exception>
        /// <param name="index">The index of the element to remove.</param>
        public void RemoveAt(int index)
        {
            ThrowIfReadOnly();

            if (index < 0 || index > Count)
                throw new ArgumentOutOfRangeException(nameof(index));

            var self = (godot_array)NativeValue;
            NativeFuncs.godotsharp_array_remove_at(ref self, index);
        }

        // ICollection

        /// <summary>
        /// Returns the number of elements in this <see cref="Array"/>.
        /// This is also known as the size or length of the array.
        /// </summary>
        /// <returns>The number of elements.</returns>
        public int Count => NativeValue.DangerousSelfRef.Size;

        bool ICollection.IsSynchronized => false;

        object ICollection.SyncRoot => false;

        /// <summary>
        /// Returns <see langword="true"/> if the array is read-only.
        /// See <see cref="MakeReadOnly"/>.
        /// </summary>
        public bool IsReadOnly => NativeValue.DangerousSelfRef.IsReadOnly;

        /// <summary>
        /// Makes the <see cref="Array"/> read-only, i.e. disabled modying of the
        /// array's elements. Does not apply to nested content, e.g. content of
        /// nested arrays.
        /// </summary>
        public void MakeReadOnly()
        {
            if (IsReadOnly)
            {
                // Avoid interop call when the array is already read-only.
                return;
            }

            var self = (godot_array)NativeValue;
            NativeFuncs.godotsharp_array_make_read_only(ref self);
        }

        /// <summary>
        /// Copies the elements of this <see cref="Array"/> to the given
        /// <see cref="Variant"/> C# array, starting at the given index.
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
        public void CopyTo(Variant[] array, int arrayIndex)
        {
            if (array == null)
                throw new ArgumentNullException(nameof(array), "Value cannot be null.");

            if (arrayIndex < 0)
            {
                throw new ArgumentOutOfRangeException(nameof(arrayIndex),
                    "Number was less than the array's lower bound in the first dimension.");
            }

            int count = Count;

            if (array.Length < (arrayIndex + count))
            {
                throw new ArgumentException(
                    "Destination array was not long enough. Check destIndex and length, and the array's lower bounds.");
            }

            unsafe
            {
                for (int i = 0; i < count; i++)
                {
                    array[arrayIndex] = Variant.CreateCopyingBorrowed(NativeValue.DangerousSelfRef.Elements[i]);
                    arrayIndex++;
                }
            }
        }

        void ICollection.CopyTo(System.Array array, int index)
        {
            if (array == null)
                throw new ArgumentNullException(nameof(array), "Value cannot be null.");

            if (index < 0)
            {
                throw new ArgumentOutOfRangeException(nameof(index),
                    "Number was less than the array's lower bound in the first dimension.");
            }

            int count = Count;

            if (array.Length < (index + count))
            {
                throw new ArgumentException(
                    "Destination array was not long enough. Check destIndex and length, and the array's lower bounds.");
            }

            unsafe
            {
                for (int i = 0; i < count; i++)
                {
                    object boxedVariant = Variant.CreateCopyingBorrowed(NativeValue.DangerousSelfRef.Elements[i]);
                    array.SetValue(boxedVariant, index);
                    index++;
                }
            }
        }

        // IEnumerable

        /// <summary>
        /// Gets an enumerator for this <see cref="Array"/>.
        /// </summary>
        /// <returns>An enumerator.</returns>
        public IEnumerator<Variant> GetEnumerator()
        {
            int count = Count;

            for (int i = 0; i < count; i++)
            {
                yield return this[i];
            }
        }

        IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();

        /// <summary>
        /// Converts this <see cref="Array"/> to a string.
        /// </summary>
        /// <returns>A string representation of this array.</returns>
        public override string ToString()
        {
            var self = (godot_array)NativeValue;
            NativeFuncs.godotsharp_array_to_string(ref self, out godot_string str);
            using (str)
                return Marshaling.ConvertStringToManaged(str);
        }

        /// <summary>
        /// The variant returned via the <paramref name="elem"/> parameter is owned by the Array and must not be disposed.
        /// </summary>
        /// <exception cref="ArgumentOutOfRangeException">
        /// <paramref name="index"/> is less than 0 or greater than the array's size.
        /// </exception>
        internal void GetVariantBorrowElementAt(int index, out godot_variant elem)
        {
            if (index < 0 || index >= Count)
                throw new ArgumentOutOfRangeException(nameof(index));
            GetVariantBorrowElementAtUnchecked(index, out elem);
        }

        /// <summary>
        /// The variant returned via the <paramref name="elem"/> parameter is owned by the Array and must not be disposed.
        /// </summary>
        internal unsafe void GetVariantBorrowElementAtUnchecked(int index, out godot_variant elem)
        {
            elem = NativeValue.DangerousSelfRef.Elements[index];
        }

        private void ThrowIfReadOnly()
        {
            if (IsReadOnly)
            {
                throw new InvalidOperationException("Array instance is read-only.");
            }
        }
    }

    internal interface IGenericGodotArray
    {
        public Array UnderlyingArray { get; }
    }

    /// <summary>
    /// Typed wrapper around Godot's Array class, an array of Variant
    /// typed elements allocated in the engine in C++. Useful when
    /// interfacing with the engine. Otherwise prefer .NET collections
    /// such as arrays or <see cref="List{T}"/>.
    /// </summary>
    /// <typeparam name="T">The type of the array.</typeparam>
    [DebuggerTypeProxy(typeof(ArrayDebugView<>))]
    [DebuggerDisplay("Count = {Count}")]
    [SuppressMessage("ReSharper", "RedundantExtendsListEntry")]
    [SuppressMessage("Design", "CA1001", MessageId = "Types that own disposable fields should be disposable",
            Justification = "Known issue. Requires explicit refcount management to not dispose untyped collections.")]
    [SuppressMessage("Naming", "CA1710", MessageId = "Identifiers should have correct suffix")]
    public sealed class Array<[MustBeVariant] T> :
        IList<T>,
        IReadOnlyList<T>,
        ICollection<T>,
        IEnumerable<T>,
        IGenericGodotArray
    {
        private static godot_variant ToVariantFunc(scoped in Array<T> godotArray) =>
            VariantUtils.CreateFromArray(godotArray);

        private static Array<T> FromVariantFunc(in godot_variant variant) =>
            VariantUtils.ConvertToArray<T>(variant);

        private void SetTypedForUnderlyingArray()
        {
            Marshaling.GetTypedCollectionParameterInfo<T>(out var elemVariantType, out var elemClassName, out var elemScriptRef);

            var self = (godot_array)NativeValue;

            using (elemScriptRef)
            {
                NativeFuncs.godotsharp_array_set_typed(
                    ref self,
                    (uint)elemVariantType,
                    elemClassName,
                    elemScriptRef);
            }
        }

        static unsafe Array()
        {
            VariantUtils.GenericConversion<Array<T>>.ToVariantCb = ToVariantFunc;
            VariantUtils.GenericConversion<Array<T>>.FromVariantCb = FromVariantFunc;
        }

        private readonly Array _underlyingArray;

        Array IGenericGodotArray.UnderlyingArray => _underlyingArray;

        internal ref godot_array.movable NativeValue
        {
            [MethodImpl(MethodImplOptions.AggressiveInlining)]
            get => ref _underlyingArray.NativeValue;
        }

        /// <summary>
        /// Constructs a new empty <see cref="Array{T}"/>.
        /// </summary>
        /// <returns>A new Godot Array.</returns>
        public Array()
        {
            _underlyingArray = new Array();
            SetTypedForUnderlyingArray();
        }

        /// <summary>
        /// Constructs a new <see cref="Array{T}"/> from the given collection's elements.
        /// </summary>
        /// <exception cref="ArgumentNullException">
        /// The <paramref name="collection"/> is <see langword="null"/>.
        /// </exception>
        /// <param name="collection">The collection of elements to construct from.</param>
        /// <returns>A new Godot Array.</returns>
        public Array(IEnumerable<T> collection)
        {
            ArgumentNullException.ThrowIfNull(collection);

            _underlyingArray = new Array();
            SetTypedForUnderlyingArray();

            foreach (T element in collection)
                Add(element);
        }

        /// <summary>
        /// Constructs a new <see cref="Array{T}"/> from the given items.
        /// </summary>
        /// <exception cref="ArgumentNullException">
        /// The <paramref name="array"/> is <see langword="null"/>.
        /// </exception>
        /// <param name="array">The items to put in the new array.</param>
        /// <returns>A new Godot Array.</returns>
        public Array(T[] array)
        {
            ArgumentNullException.ThrowIfNull(array);

            _underlyingArray = new Array();
            SetTypedForUnderlyingArray();

            foreach (T element in array)
                Add(element);
        }

        /// <summary>
        /// Constructs a typed <see cref="Array{T}"/> from an untyped <see cref="Array"/>.
        /// </summary>
        /// <exception cref="ArgumentNullException">
        /// The <paramref name="array"/> is <see langword="null"/>.
        /// </exception>
        /// <param name="array">The untyped array to construct from.</param>
        /// <returns>A new Godot Array.</returns>
        public Array(Array array)
        {
            ArgumentNullException.ThrowIfNull(array);

            _underlyingArray = array;
        }

        // Explicit name to make it very clear
        internal static Array<T> CreateTakingOwnershipOfDisposableValue(godot_array nativeValueToOwn)
            => new Array<T>(Array.CreateTakingOwnershipOfDisposableValue(nativeValueToOwn));

        /// <summary>
        /// Converts this typed <see cref="Array{T}"/> to an untyped <see cref="Array"/>.
        /// </summary>
        /// <param name="from">The typed array to convert.</param>
        /// <returns>A new Godot Array, or <see langword="null"/> if <see paramref="from"/> was null.</returns>
        [return: NotNullIfNotNull("from")]
        public static explicit operator Array?(Array<T>? from)
        {
            return from?._underlyingArray;
        }

        /// <summary>
        /// Duplicates this <see cref="Array{T}"/>.
        /// </summary>
        /// <param name="deep">If <see langword="true"/>, performs a deep copy.</param>
        /// <returns>A new Godot Array.</returns>
        public Array<T> Duplicate(bool deep = false)
        {
            return new Array<T>(_underlyingArray.Duplicate(deep));
        }

        /// <summary>
        /// Assigns the given value to all elements in the array. This can typically be
        /// used together with <see cref="Resize(int)"/> to create an array with a given
        /// size and initialized elements.
        /// Note: If <paramref name="value"/> is of a reference type (<see cref="GodotObject"/>
        /// derived, <see cref="Array"/> or <see cref="Dictionary"/>, etc.) then the array
        /// is filled with the references to the same object, i.e. no duplicates are
        /// created.
        /// </summary>
        /// <example>
        /// <code>
        /// var array = new Godot.Collections.Array&lt;int&gt;();
        /// array.Resize(10);
        /// array.Fill(0); // Initialize the 10 elements to 0.
        /// </code>
        /// </example>
        /// <exception cref="InvalidOperationException">
        /// The array is read-only.
        /// </exception>
        /// <param name="value">The value to fill the array with.</param>
        public void Fill(T value)
        {
            ThrowIfReadOnly();

            godot_variant variantValue = VariantUtils.CreateFrom(value);
            var self = (godot_array)_underlyingArray.NativeValue;
            NativeFuncs.godotsharp_array_fill(ref self, variantValue);
        }

        /// <summary>
        /// Returns the maximum value contained in the array if all elements are of
        /// comparable types. If the elements can't be compared, <see langword="default"/>
        /// is returned.
        /// </summary>
        /// <returns>The maximum value contained in the array.</returns>
        public T Max()
        {
            godot_variant resVariant;
            var self = (godot_array)_underlyingArray.NativeValue;
            NativeFuncs.godotsharp_array_max(ref self, out resVariant);
            return VariantUtils.ConvertTo<T>(resVariant);
        }

        /// <summary>
        /// Returns the minimum value contained in the array if all elements are of
        /// comparable types. If the elements can't be compared, <see langword="default"/>
        /// is returned.
        /// </summary>
        /// <returns>The minimum value contained in the array.</returns>
        public T Min()
        {
            godot_variant resVariant;
            var self = (godot_array)_underlyingArray.NativeValue;
            NativeFuncs.godotsharp_array_min(ref self, out resVariant);
            return VariantUtils.ConvertTo<T>(resVariant);
        }

        /// <summary>
        /// Returns a random value from the target array.
        /// </summary>
        /// <example>
        /// <code>
        /// var array = new Godot.Collections.Array&lt;int&gt; { 1, 2, 3, 4 };
        /// GD.Print(array.PickRandom()); // Prints either of the four numbers.
        /// </code>
        /// </example>
        /// <returns>A random element from the array.</returns>
        public T PickRandom()
        {
            godot_variant resVariant;
            var self = (godot_array)_underlyingArray.NativeValue;
            NativeFuncs.godotsharp_array_pick_random(ref self, out resVariant);
            return VariantUtils.ConvertTo<T>(resVariant);
        }

        /// <summary>
        /// Compares this <see cref="Array{T}"/> against the <paramref name="other"/>
        /// <see cref="Array{T}"/> recursively. Returns <see langword="true"/> if the
        /// sizes and contents of the arrays are equal, <see langword="false"/>
        /// otherwise.
        /// </summary>
        /// <param name="other">The other array to compare against.</param>
        /// <returns>
        /// <see langword="true"/> if the sizes and contents of the arrays are equal,
        /// <see langword="false"/> otherwise.
        /// </returns>
        public bool RecursiveEqual(Array<T> other)
        {
            return _underlyingArray.RecursiveEqual(other._underlyingArray);
        }

        /// <summary>
        /// Resizes this <see cref="Array{T}"/> to the given size.
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// The array is read-only.
        /// </exception>
        /// <param name="newSize">The new size of the array.</param>
        /// <returns><see cref="Error.Ok"/> if successful, or an error code.</returns>
        public Error Resize(int newSize)
        {
            return _underlyingArray.Resize(newSize);
        }

        /// <summary>
        /// Reverses the order of the elements in the array.
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// The array is read-only.
        /// </exception>
        public void Reverse()
        {
            _underlyingArray.Reverse();
        }

        /// <summary>
        /// Shuffles the array such that the items will have a random order.
        /// This method uses the global random number generator common to methods
        /// such as <see cref="GD.Randi"/>. Call <see cref="GD.Randomize"/> to
        /// ensure that a new seed will be used each time if you want
        /// non-reproducible shuffling.
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// The array is read-only.
        /// </exception>
        public void Shuffle()
        {
            _underlyingArray.Shuffle();
        }

        /// <summary>
        /// Creates a shallow copy of a range of elements in the source <see cref="Array{T}"/>.
        /// </summary>
        /// <exception cref="ArgumentOutOfRangeException">
        /// <paramref name="start"/> is less than 0 or greater than the array's size.
        /// </exception>
        /// <param name="start">The zero-based index at which the range starts.</param>
        /// <returns>A new array that contains the elements inside the slice range.</returns>
        public Array<T> Slice(int start)
        {
            return GetSliceRange(start, Count, step: 1, deep: false);
        }

        /// <summary>
        /// Creates a shallow copy of a range of elements in the source <see cref="Array{T}"/>.
        /// </summary>
        /// <exception cref="ArgumentOutOfRangeException">
        /// <paramref name="start"/> is less than 0 or greater than the array's size.
        /// -or-
        /// <paramref name="length"/> is less than 0 or greater than the array's size.
        /// </exception>
        /// <param name="start">The zero-based index at which the range starts.</param>
        /// <param name="length">The length of the range.</param>
        /// <returns>A new array that contains the elements inside the slice range.</returns>
        // The Slice method must have this signature to get implicit Range support.
        public Array<T> Slice(int start, int length)
        {
            return GetSliceRange(start, start + length, step: 1, deep: false);
        }

        /// <summary>
        /// Returns the slice of the <see cref="Array{T}"/>, from <paramref name="start"/>
        /// (inclusive) to <paramref name="end"/> (exclusive), as a new <see cref="Array{T}"/>.
        /// The absolute value of <paramref name="start"/> and <paramref name="end"/>
        /// will be clamped to the array size.
        /// If either <paramref name="start"/> or <paramref name="end"/> are negative, they
        /// will be relative to the end of the array (i.e. <c>arr.GetSliceRange(0, -2)</c>
        /// is a shorthand for <c>arr.GetSliceRange(0, arr.Count - 2)</c>).
        /// If specified, <paramref name="step"/> is the relative index between source
        /// elements. It can be negative, then <paramref name="start"/> must be higher than
        /// <paramref name="end"/>. For example, <c>[0, 1, 2, 3, 4, 5].GetSliceRange(5, 1, -2)</c>
        /// returns <c>[5, 3]</c>.
        /// If <paramref name="deep"/> is true, each element will be copied by value
        /// rather than by reference.
        /// </summary>
        /// <param name="start">The zero-based index at which the range starts.</param>
        /// <param name="end">The zero-based index at which the range ends.</param>
        /// <param name="step">The relative index between source elements to take.</param>
        /// <param name="deep">If <see langword="true"/>, performs a deep copy.</param>
        /// <returns>A new array that contains the elements inside the slice range.</returns>
        public Array<T> GetSliceRange(int start, int end, int step = 1, bool deep = false)
        {
            return new Array<T>(_underlyingArray.GetSliceRange(start, end, step, deep));
        }

        /// <summary>
        /// Sorts the array.
        /// Note: The sorting algorithm used is not stable. This means that values
        /// considered equal may have their order changed when using <see cref="Sort"/>.
        /// Note: Strings are sorted in alphabetical order (as opposed to natural order).
        /// This may lead to unexpected behavior when sorting an array of strings ending
        /// with a sequence of numbers.
        /// To sort with a custom predicate use
        /// <see cref="Enumerable.OrderBy{TSource, TKey}(IEnumerable{TSource}, Func{TSource, TKey})"/>.
        /// </summary>
        /// <example>
        /// <code>
        /// var strings = new Godot.Collections.Array&lt;string&gt; { "string1", "string2", "string10", "string11" };
        /// strings.Sort();
        /// GD.Print(strings); // Prints [string1, string10, string11, string2]
        /// </code>
        /// </example>
        /// <exception cref="InvalidOperationException">
        /// The array is read-only.
        /// </exception>
        public void Sort()
        {
            _underlyingArray.Sort();
        }

        /// <summary>
        /// Concatenates two <see cref="Array{T}"/>s together, with the <paramref name="right"/>
        /// being added to the end of the <see cref="Array{T}"/> specified in <paramref name="left"/>.
        /// For example, <c>[1, 2] + [3, 4]</c> results in <c>[1, 2, 3, 4]</c>.
        /// </summary>
        /// <param name="left">The first array.</param>
        /// <param name="right">The second array.</param>
        /// <returns>A new Godot Array with the contents of both arrays.</returns>
        public static Array<T> operator +(Array<T> left, Array<T> right)
        {
            if (left == null)
            {
                if (right == null)
                    return new Array<T>();

                return right.Duplicate(deep: false);
            }

            if (right == null)
                return left.Duplicate(deep: false);

            return new Array<T>(left._underlyingArray + right._underlyingArray);
        }

        // IList<T>

        /// <summary>
        /// Returns the item at the given <paramref name="index"/>.
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// The property is assigned and the array is read-only.
        /// </exception>
        /// <exception cref="ArgumentOutOfRangeException">
        /// <paramref name="index"/> is less than 0 or greater than the array's size.
        /// </exception>
        /// <value>The <see cref="Variant"/> item at the given <paramref name="index"/>.</value>
        public unsafe T this[int index]
        {
            get
            {
                _underlyingArray.GetVariantBorrowElementAt(index, out godot_variant borrowElem);
                return VariantUtils.ConvertTo<T>(borrowElem);
            }
            set
            {
                ThrowIfReadOnly();

                if (index < 0 || index >= Count)
                    throw new ArgumentOutOfRangeException(nameof(index));

                var self = (godot_array)_underlyingArray.NativeValue;
                godot_variant* ptrw = NativeFuncs.godotsharp_array_ptrw(ref self);
                godot_variant* itemPtr = &ptrw[index];
                (*itemPtr).Dispose();
                *itemPtr = VariantUtils.CreateFrom(value);
            }
        }

        /// <summary>
        /// Searches the array for a value and returns its index or <c>-1</c> if not found.
        /// </summary>
        /// <param name="item">The <see cref="Variant"/> item to search for.</param>
        /// <returns>The index of the item, or -1 if not found.</returns>
        public int IndexOf(T item)
        {
            if (Count == 0)
            {
                // Special case for empty array to avoid an interop call.
                return -1;
            }

            using var variantValue = VariantUtils.CreateFrom(item);
            var self = (godot_array)_underlyingArray.NativeValue;
            return NativeFuncs.godotsharp_array_index_of(ref self, variantValue);
        }

        /// <summary>
        /// Searches the array for a value and returns its index or <c>-1</c> if not found.
        /// </summary>
        /// <exception cref="ArgumentOutOfRangeException">
        /// <paramref name="index"/> is less than 0 or greater than the array's size.
        /// </exception>
        /// <param name="item">The <see cref="Variant"/> item to search for.</param>
        /// <param name="index">The initial search index to start from.</param>
        /// <returns>The index of the item, or -1 if not found.</returns>
        public int IndexOf(T item, int index)
        {
            if (index < 0 || index > Count)
                throw new ArgumentOutOfRangeException(nameof(index));

            if (Count == 0)
            {
                // Special case for empty array to avoid an interop call.
                return -1;
            }

            godot_variant variantValue = VariantUtils.CreateFrom(item);
            var self = (godot_array)_underlyingArray.NativeValue;
            return NativeFuncs.godotsharp_array_index_of(ref self, variantValue, index);
        }

        /// <summary>
        /// Searches the array for a value in reverse order and returns its index
        /// or <c>-1</c> if not found.
        /// </summary>
        /// <param name="item">The <see cref="Variant"/> item to search for.</param>
        /// <returns>The index of the item, or -1 if not found.</returns>
        public int LastIndexOf(Variant item)
        {
            if (Count == 0)
            {
                // Special case for empty array to avoid an interop call.
                return -1;
            }

            godot_variant variantValue = VariantUtils.CreateFrom(item);
            var self = (godot_array)_underlyingArray.NativeValue;
            return NativeFuncs.godotsharp_array_last_index_of(ref self, variantValue, Count - 1);
        }

        /// <summary>
        /// Searches the array for a value in reverse order and returns its index
        /// or <c>-1</c> if not found.
        /// </summary>
        /// <exception cref="ArgumentOutOfRangeException">
        /// <paramref name="index"/> is less than 0 or greater than the array's size.
        /// </exception>
        /// <param name="item">The <see cref="Variant"/> item to search for.</param>
        /// <param name="index">The initial search index to start from.</param>
        /// <returns>The index of the item, or -1 if not found.</returns>
        public int LastIndexOf(Variant item, int index)
        {
            if (index < 0 || index >= Count)
                throw new ArgumentOutOfRangeException(nameof(index));

            if (Count == 0)
            {
                // Special case for empty array to avoid an interop call.
                return -1;
            }

            godot_variant variantValue = VariantUtils.CreateFrom(item);
            var self = (godot_array)_underlyingArray.NativeValue;
            return NativeFuncs.godotsharp_array_last_index_of(ref self, variantValue, index);
        }

        /// <summary>
        /// Inserts a new element at a given position in the array. The position
        /// must be valid, or at the end of the array (<c>pos == Count - 1</c>).
        /// Existing items will be moved to the right.
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// The array is read-only.
        /// </exception>
        /// <exception cref="ArgumentOutOfRangeException">
        /// <paramref name="index"/> is less than 0 or greater than the array's size.
        /// </exception>
        /// <param name="index">The index to insert at.</param>
        /// <param name="item">The <see cref="Variant"/> item to insert.</param>
        public void Insert(int index, T item)
        {
            ThrowIfReadOnly();

            if (index < 0 || index > Count)
                throw new ArgumentOutOfRangeException(nameof(index));

            using var variantValue = VariantUtils.CreateFrom(item);
            var self = (godot_array)_underlyingArray.NativeValue;
            NativeFuncs.godotsharp_array_insert(ref self, index, variantValue);
        }

        /// <summary>
        /// Removes an element from the array by index.
        /// To remove an element by searching for its value, use
        /// <see cref="Remove(T)"/> instead.
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// The array is read-only.
        /// </exception>
        /// <exception cref="ArgumentOutOfRangeException">
        /// <paramref name="index"/> is less than 0 or greater than the array's size.
        /// </exception>
        /// <param name="index">The index of the element to remove.</param>
        public void RemoveAt(int index)
        {
            _underlyingArray.RemoveAt(index);
        }

        // ICollection<T>

        /// <summary>
        /// Returns the number of elements in this <see cref="Array{T}"/>.
        /// This is also known as the size or length of the array.
        /// </summary>
        /// <returns>The number of elements.</returns>
        public int Count => _underlyingArray.Count;

        /// <summary>
        /// Returns <see langword="true"/> if the array is read-only.
        /// See <see cref="MakeReadOnly"/>.
        /// </summary>
        public bool IsReadOnly => _underlyingArray.IsReadOnly;

        /// <summary>
        /// Makes the <see cref="Array{T}"/> read-only, i.e. disabled modying of the
        /// array's elements. Does not apply to nested content, e.g. content of
        /// nested arrays.
        /// </summary>
        public void MakeReadOnly()
        {
            _underlyingArray.MakeReadOnly();
        }

        /// <summary>
        /// Adds an item to the end of this <see cref="Array{T}"/>.
        /// This is the same as <c>append</c> or <c>push_back</c> in GDScript.
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// The array is read-only.
        /// </exception>
        /// <param name="item">The <see cref="Variant"/> item to add.</param>
        public void Add(T item)
        {
            ThrowIfReadOnly();

            using var variantValue = VariantUtils.CreateFrom(item);
            var self = (godot_array)_underlyingArray.NativeValue;
            _ = NativeFuncs.godotsharp_array_add(ref self, variantValue);
        }

        /// <summary>
        /// Adds the elements of the specified collection to the end of this <see cref="Array{T}"/>.
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// The array is read-only.
        /// </exception>
        /// <exception cref="ArgumentNullException">
        /// The <paramref name="collection"/> is <see langword="null"/>.
        /// </exception>
        /// <param name="collection">Collection of <see cref="Variant"/> items to add.</param>
        public void AddRange(IEnumerable<T> collection)
        {
            ThrowIfReadOnly();

            if (collection == null)
                throw new ArgumentNullException(nameof(collection), "Value cannot be null.");

            // If the collection is another Godot Array, we can add the items
            // with a single interop call.
            if (collection is Array array)
            {
                var self = (godot_array)_underlyingArray.NativeValue;
                var collectionNative = (godot_array)array.NativeValue;
                _ = NativeFuncs.godotsharp_array_add_range(ref self, collectionNative);
                return;
            }
            if (collection is Array<T> typedArray)
            {
                var self = (godot_array)_underlyingArray.NativeValue;
                var collectionNative = (godot_array)typedArray._underlyingArray.NativeValue;
                _ = NativeFuncs.godotsharp_array_add_range(ref self, collectionNative);
                return;
            }

            // If we can retrieve the count of the collection without enumerating it
            // (e.g.: the collections is a List<T>), use it to resize the array once
            // instead of growing it as we add items.
            if (collection.TryGetNonEnumeratedCount(out int count))
            {
                int oldCount = Count;
                Resize(Count + count);

                using var enumerator = collection.GetEnumerator();

                for (int i = 0; i < count; i++)
                {
                    enumerator.MoveNext();
                    this[oldCount + i] = enumerator.Current;
                }

                return;
            }

            foreach (var item in collection)
            {
                Add(item);
            }
        }

        /// <summary>
        /// Finds the index of an existing value using binary search.
        /// If the value is not present in the array, it returns the bitwise
        /// complement of the insertion index that maintains sorting order.
        /// Note: Calling <see cref="BinarySearch(int, int, T)"/> on an unsorted
        /// array results in unexpected behavior.
        /// </summary>
        /// <exception cref="ArgumentOutOfRangeException">
        /// <paramref name="index"/> is less than 0.
        /// -or-
        /// <paramref name="count"/> is less than 0.
        /// </exception>
        /// <exception cref="ArgumentException">
        /// <paramref name="index"/> and <paramref name="count"/> do not denote
        /// a valid range in the <see cref="Array{T}"/>.
        /// </exception>
        /// <param name="index">The starting index of the range to search.</param>
        /// <param name="count">The length of the range to search.</param>
        /// <param name="item">The object to locate.</param>
        /// <returns>
        /// The index of the item in the array, if <paramref name="item"/> is found;
        /// otherwise, a negative number that is the bitwise complement of the index
        /// of the next element that is larger than <paramref name="item"/> or, if
        /// there is no larger element, the bitwise complement of <see cref="Count"/>.
        /// </returns>
        public int BinarySearch(int index, int count, T item)
        {
            if (index < 0)
                throw new ArgumentOutOfRangeException(nameof(index), "index cannot be negative.");
            if (count < 0)
                throw new ArgumentOutOfRangeException(nameof(count), "count cannot be negative.");
            if (Count - index < count)
                throw new ArgumentException("length is out of bounds or count is greater than the number of elements.");

            if (Count == 0)
            {
                // Special case for empty array to avoid an interop call.
                return -1;
            }

            using var variantValue = VariantUtils.CreateFrom(item);
            var self = (godot_array)_underlyingArray.NativeValue;
            return NativeFuncs.godotsharp_array_binary_search(ref self, index, count, variantValue);
        }

        /// <summary>
        /// Finds the index of an existing value using binary search.
        /// If the value is not present in the array, it returns the bitwise
        /// complement of the insertion index that maintains sorting order.
        /// Note: Calling <see cref="BinarySearch(T)"/> on an unsorted
        /// array results in unexpected behavior.
        /// </summary>
        /// <param name="item">The object to locate.</param>
        /// <returns>
        /// The index of the item in the array, if <paramref name="item"/> is found;
        /// otherwise, a negative number that is the bitwise complement of the index
        /// of the next element that is larger than <paramref name="item"/> or, if
        /// there is no larger element, the bitwise complement of <see cref="Count"/>.
        /// </returns>
        public int BinarySearch(T item)
        {
            return BinarySearch(0, Count, item);
        }

        /// <summary>
        /// Clears the array. This is the equivalent to using <see cref="Resize(int)"/>
        /// with a size of <c>0</c>
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// The array is read-only.
        /// </exception>
        public void Clear()
        {
            _underlyingArray.Clear();
        }

        /// <summary>
        /// Returns <see langword="true"/> if the array contains the given value.
        /// </summary>
        /// <example>
        /// <code>
        /// var arr = new Godot.Collections.Array&lt;string&gt; { "inside", "7" };
        /// GD.Print(arr.Contains("inside")); // True
        /// GD.Print(arr.Contains("outside")); // False
        /// GD.Print(arr.Contains(7)); // False
        /// GD.Print(arr.Contains("7")); // True
        /// </code>
        /// </example>
        /// <param name="item">The item to look for.</param>
        /// <returns>Whether or not this array contains the given item.</returns>
        public bool Contains(T item) => IndexOf(item) != -1;

        /// <summary>
        /// Copies the elements of this <see cref="Array{T}"/> to the given
        /// C# array, starting at the given index.
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
        /// <param name="array">The C# array to copy to.</param>
        /// <param name="arrayIndex">The index to start at.</param>
        public void CopyTo(T[] array, int arrayIndex)
        {
            if (array == null)
                throw new ArgumentNullException(nameof(array), "Value cannot be null.");

            if (arrayIndex < 0)
            {
                throw new ArgumentOutOfRangeException(nameof(arrayIndex),
                    "Number was less than the array's lower bound in the first dimension.");
            }

            int count = Count;

            if (array.Length < (arrayIndex + count))
            {
                throw new ArgumentException(
                    "Destination array was not long enough. Check destIndex and length, and the array's lower bounds.");
            }

            for (int i = 0; i < count; i++)
            {
                array[arrayIndex] = this[i];
                arrayIndex++;
            }
        }

        /// <summary>
        /// Removes the first occurrence of the specified <paramref name="item"/>
        /// from this <see cref="Array{T}"/>.
        /// </summary>
        /// <exception cref="InvalidOperationException">
        /// The array is read-only.
        /// </exception>
        /// <param name="item">The value to remove.</param>
        /// <returns>A <see langword="bool"/> indicating success or failure.</returns>
        public bool Remove(T item)
        {
            ThrowIfReadOnly();

            int index = IndexOf(item);
            if (index >= 0)
            {
                RemoveAt(index);
                return true;
            }

            return false;
        }

        // IEnumerable<T>

        /// <summary>
        /// Gets an enumerator for this <see cref="Array{T}"/>.
        /// </summary>
        /// <returns>An enumerator.</returns>
        public IEnumerator<T> GetEnumerator()
        {
            int count = _underlyingArray.Count;

            for (int i = 0; i < count; i++)
            {
                yield return this[i];
            }
        }

        IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();

        /// <summary>
        /// Converts this <see cref="Array{T}"/> to a string.
        /// </summary>
        /// <returns>A string representation of this array.</returns>
        public override string ToString() => _underlyingArray.ToString();

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static implicit operator Variant(Array<T> from) => Variant.CreateFrom(from);

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static explicit operator Array<T>(Variant from) => from.AsGodotArray<T>();

        private void ThrowIfReadOnly()
        {
            if (IsReadOnly)
            {
                throw new InvalidOperationException("Array instance is read-only.");
            }
        }
    }
}
