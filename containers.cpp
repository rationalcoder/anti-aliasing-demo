
template <typename T_, u32 BucketSize_> static T_*
flatten(const Bucket_List<T_, BucketSize_>& list)
{
    T_* flat = allocate_array(list.size(), T_);

    u32 i = 0;
    for (auto& e : list)
        flat[i++] = e;

    return flat;
}
