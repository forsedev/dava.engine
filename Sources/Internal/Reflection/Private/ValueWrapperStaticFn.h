#pragma once
#include "../ReflectionWrappers.h"

namespace DAVA
{
template <typename GetT, typename SetT>
class ValueWrapperStaticFn : public ValueWrapper
{
    using Getter = GetT (*)();
    using Setter = void (*)(SetT);

public:
    ValueWrapperStaticFn(Getter getter_, Setter setter_ = nullptr)
        : ValueWrapper()
        , getter(getter_)
        , setter(setter_)
    {
    }

    bool IsReadonly() const override
    {
        return (nullptr == setter);
    }

    const Type* GetType() const override
    {
        return Type::Instance<GetT>();
    }

    Any GetValue(const ReflectedObject& object) const override
    {
        using UnrefGetT = typename std::remove_reference<GetT>::type;

        Any ret;
        UnrefGetT v = (*getter)();
        ret.Set(std::move(v));
        return ret;
    }

    bool SetValue(const ReflectedObject& object, const Any& value) const override
    {
        bool ret = false;

        if (nullptr != setter)
        {
            const SetT& v = value.Get<SetT>();
            (*setter)(v);

            ret = true;
        }

        return ret;
    }

    ReflectedObject GetValueObject(const ReflectedObject& object) const override
    {
        auto is_pointer = std::integral_constant<bool, std::is_pointer<GetT>::value>();
        auto is_reference = std::integral_constant<bool, std::is_reference<GetT>::value>();

        return GetValueObjectImpl(object, is_pointer, is_reference);
    }

protected:
    Getter getter = nullptr;
    Setter setter = nullptr;

private:
    inline ReflectedObject GetValueObjectImpl(const ReflectedObject& object, std::false_type /* is_pointer */, std::false_type /* is_reference */) const
    {
        return ReflectedObject();
    }

    inline ReflectedObject GetValueObjectImpl(const ReflectedObject& object, std::true_type /* is_pointer */, std::false_type /* is_reference */) const
    {
        return ReflectedObject((*getter)());
    }

    inline ReflectedObject GetValueObjectImpl(const ReflectedObject& object, std::false_type /* is_pointer */, std::true_type /* is_reference */) const
    {
        GetT v = (*getter)();
        return ReflectedObject(&v);
    }
};

} // namespace DAVA
