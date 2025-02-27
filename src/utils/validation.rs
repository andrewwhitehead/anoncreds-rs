use crate::error::ValidationError;
use once_cell::sync::Lazy;
use regex::Regex;

// TODO: stricten the URI regex.
// Right now everything after the first colon is allowed,
// we might want to restrict this
pub static URI_IDENTIFIER: Lazy<Regex> =
    Lazy::new(|| Regex::new(r"^[a-zA-Z0-9\+\-\.]+:.+$").unwrap());

/// base58 alpahet as defined in
/// https://datatracker.ietf.org/doc/html/draft-msporny-base58#section-2
/// This is used for legacy indy identifiers that we will keep supporting for
/// backwards compatibility. This might validate invalid identifiers if they happen
/// to fall within the base58 alphabet, but there is not much we can do about that.
pub static LEGACY_IDENTIFIER: Lazy<Regex> =
    Lazy::new(|| Regex::new("^[1-9A-HJ-NP-Za-km-z]{21,22}$").unwrap());

pub fn is_uri_identifier(id: &str) -> bool {
    URI_IDENTIFIER.captures(id).is_some()
}

/// Macro to return a new `ValidationError` with an optional message
#[macro_export]
macro_rules! invalid {
    () => { $crate::error::ValidationError::from(None) };
    ($($arg:tt)+) => {
        $crate::error::ValidationError::from(format!($($arg)+))
    };
}

/// Trait for data types which need validation after being loaded from external sources
/// TODO: this should not default to Ok(())
pub trait Validatable {
    fn validate(&self) -> Result<(), ValidationError> {
        Ok(())
    }
}
