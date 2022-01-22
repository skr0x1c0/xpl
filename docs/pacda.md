#  Signing using `PACDA`

## Steps followed to sign a pointer `ptr` with context `ctx` using `DA` key

1. Create an `IOSurface` and add 64512 key value properties to it
2. Acquire lock on `KHEAP_DEFAULT.kh_large_map.lock`
3. Asynchronously add one more key value property to `IOSurface` created in step 1
4. Find thread in current process with state `TH_WAIT | TH_UNINT`. This is the thread that is asynchronously processing step 3
5. Modify the kernel stack of thread found in step 4 such that the value of registers X20 and X23 after `kfree_ext` returns will be equal to `ptr` and `ctx` respectively
6. Release the lock `KHEAP_DEFAULT.kh_large_map.lock`
7. Read the signed pointer from instance variable `dictionary` of the `IOSurface` props dictionary


## Working

The instance variable `dictionary` of class `OSDictionary` is signed using `DA` key. This variable stores the base pointer of array storing keys and values of the dictionary. The memory backing this array is dynamically allocated from `KHEAP_DEFAULT` based on the capacity of dictionary.

``` c
class OSDictionary : public OSCollection
{
    friend class OSSerialize;

    OSDeclareDefaultStructors(OSDictionary);

#if APPLE_KEXT_ALIGN_CONTAINERS

protected:
    ...
    struct dictEntry {
        OSTaggedPtr<const OSSymbol>        key;
        OSTaggedPtr<const OSMetaClassBase> value;
#if XNU_KERNEL_PRIVATE
        static int compare(const void *, const void *);
#endif
    };
    dictEntry    * OS_PTRAUTH_SIGNED_PTR("OSDictionary.dictionary") dictionary;
    ...
};
```

When a new value is added to the dictionary using `OSDictionary::setObject` method, the method calls `OSDictionary::ensureCapacity` method to allocate a larger `dictionary` array if the dictionary is full 

``` c
bool
OSDictionary::
setObject(const OSSymbol *aKey, const OSMetaClassBase *anObject, bool onlyAdd)
{
    unsigned int i;
    bool exists;

    if (!anObject || !aKey) {
        return false;
    }

    // if the key exists, replace the object
    ...

    if (exists) {
        ...
        return true;
    }

    // add new key, possibly extending our capacity
    if (count >= capacity && count >= ensureCapacity(count + 1)) {
        return false;
    }
    ...
    return true;
}
```

``` c
unsigned int
OSDictionary::ensureCapacity(unsigned int newCapacity)
{
    dictEntry *newDict;
    vm_size_t finalCapacity;
    vm_size_t oldSize, newSize;

    if (newCapacity <= capacity) {
        return capacity;
    }

    // round up
    finalCapacity = (((newCapacity - 1) / capacityIncrement) + 1)
        * capacityIncrement;

    // integer overflow check
    if (finalCapacity < newCapacity) {
        return capacity;
    }

    newSize = sizeof(dictEntry) * finalCapacity;

    newDict = (dictEntry *) kallocp_container(&newSize); // Stored in X20 register
    if (newDict) {
        // use all of the actual allocation size
        finalCapacity = (newSize / sizeof(dictEntry));
        if (finalCapacity > UINT_MAX) {
            // failure, too large
            kfree(newDict, newSize);
            return capacity;
        }

        oldSize = sizeof(dictEntry) * capacity;

        os::uninitialized_move(dictionary, dictionary + capacity, newDict);
        os::uninitialized_value_construct(newDict + capacity, newDict + finalCapacity);
        os::destroy(dictionary, dictionary + capacity);

        OSCONTAINER_ACCUMSIZE(((size_t)newSize) - ((size_t)oldSize));
        kfree(dictionary, oldSize);

        dictionary = newDict;
        capacity = (unsigned int) finalCapacity;
    }

    return capacity;
}
```
